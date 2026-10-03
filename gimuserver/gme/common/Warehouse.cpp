#include "App.hpp"
#include "Common.hpp"

#include <map>
#include <memory>

namespace gme
{
namespace
{
struct Row
{
    int64_t id, item, count, favorite, order, unit, slot;
};

uint32_t maxStack(const int64_t item)
{
    const auto& mst = theServer()->cache().itemMst();
    const auto it = std::find_if(mst.begin(), mst.end(),
        [item](const auto& row) { return row.id == item; });
    return it == mst.end() || it->max_stack <= 0 ? UINT32_MAX : static_cast<uint32_t>(it->max_stack);
}

// All callers hold one transaction. user_items is the aggregate warehouse
// quantity (equipped spheres excluded); rows are its persistent presentation.
// Generic species-level spenders consume unfavorited rows first. Exact row
// operations update both stores explicitly so no surviving ID has to move.
drogon::Task<void> reconcile(const db::Database database, const UserIdentity identity)
{
    const auto stocks = co_await database->execSqlCoro(
        "SELECT item_id,item_num,favorite_flg,disp_order FROM user_items WHERE user_id=$1;", identity.userId);
    struct Stock { int64_t count=0, favorite=0, order=0; };
    std::map<int64_t, Stock> totals;
    for (const auto& s : stocks)
        totals[s["item_id"].as<int64_t>()] = { s["item_num"].as<int64_t>(),
            s["favorite_flg"].as<int64_t>(), s["disp_order"].as<int64_t>() };

    const auto units = co_await database->execSqlCoro(
        "SELECT user_unit_id,eqip_item_id,eqip_item_id2,eqip_item_frame_id,eqip_item_frame_id2"
        " FROM user_units WHERE user_id=$1;", identity.userId);
    std::map<std::pair<int64_t,int64_t>, int64_t> equipped;
    // What each slot's frame says now, and the row it has to name.  See the
    // frame repair at the end of this function.
    std::map<std::pair<int64_t,int64_t>, int64_t> frames;
    std::map<std::pair<int64_t,int64_t>, int64_t> wearing;
    for (const auto& u : units)
        for (int slot=1; slot<=2; ++slot)
        {
            const std::pair<int64_t,int64_t> where{u["user_unit_id"].as<int64_t>(),slot};
            const auto item=u[slot==1 ? "eqip_item_id" : "eqip_item_id2"].as<int64_t>();
            frames[where]=u[slot==1 ? "eqip_item_frame_id" : "eqip_item_frame_id2"].as<int64_t>();
            if (item>0) equipped[where]=item;
        }

    std::map<int64_t,std::vector<Row>> rows;
    // LIVE ROWS ONLY.  A tombstone (item_num 0, not on a unit) counts for
    // nothing below -- it adds 0 to `have`, is never taken from and is never
    // topped up, because a spent id is not reused -- but a long-lived save
    // piles them up (every copy sold or spent leaves one), and reading them
    // here made a 3200-copy warehouse with 20000 tombstones take ~6x as long.
    // The predicate is spelt exactly as warehouse_live_rows' so that partial
    // index serves it (30092026_WarehouseLiveRowIndex).
    const auto saved = co_await database->execSqlCoro(
        "SELECT * FROM user_warehouse_rows WHERE user_id=$1 AND (item_num>0 OR equip_unit_id!=0)"
        " ORDER BY instance_id;", identity.userId);
    for (const auto& s : saved)
    {
        Row r{s["instance_id"].as<int64_t>(),s["item_id"].as<int64_t>(),s["item_num"].as<int64_t>(),
            s["favorite_flg"].as<int64_t>(),s["disp_order"].as<int64_t>(),
            s["equip_unit_id"].as<int64_t>(),s["equip_slot"].as<int64_t>()};
        totals.try_emplace(r.item);
        if (r.unit)
        {
            const auto it=equipped.find({r.unit,r.slot});
            if (it!=equipped.end() && it->second==r.item)
            {
                wearing[it->first]=r.id;
                equipped.erase(it);
            }
            else
            {
                // A legacy species-level operation returned/deleted the unit.
                // Keep this copy's identity and lock; stock reconciliation below
                // determines whether a returned copy is actually available.
                r.unit=0; r.slot=0;
                co_await database->execSqlCoro(
                    "UPDATE user_warehouse_rows SET equip_unit_id=0,equip_slot=0 WHERE instance_id=$1;",r.id);
            }
        }
        rows[r.item].push_back(r);
    }

    // Existing saves store equipped spheres only on units. Give each such
    // sphere its own zero-quantity row without subtracting warehouse stock.
    for (const auto& [where,item] : equipped)
    {
        const auto added=co_await database->execSqlCoro(
            "INSERT INTO user_warehouse_rows(user_id,item_id,item_num,favorite_flg,equip_unit_id,equip_slot)"
            " VALUES ($1,$2,0,$3,$4,$5) RETURNING instance_id;",
            identity.userId,item,totals[item].favorite,where.first,where.second);
        wearing[where]=added[0]["instance_id"].as<int64_t>();
        rows[item].push_back({added[0]["instance_id"].as<int64_t>(),item,0,
            totals[item].favorite,0,where.first,where.second});
    }

    // A WORN SLOT'S FRAME NAMES ITS COPY'S ROW, an empty one says 0.  The
    // client keys "which unit wears this row" on the frame
    // (UserUnitInfoList::updateSphereEquipList @0x12BA7F8), and its next equip
    // batch names an unchanged slot's copy by it.  Saves from before
    // 2026-10-02 hold the sphere's ItemMst.sphere_type there instead, which
    // marked whatever row had that index as worn and got any later batch
    // carrying the slot refused as "copy unavailable".  Repaired here, where
    // the worn rows are matched, so every save converges on its next sync.
    // -1 (no second slot) is never touched: a repair must not grant a slot.
    for (const auto& [where,frame] : frames)
    {
        if (frame<0) continue;
        const auto it=wearing.find(where);
        const int64_t want=it!=wearing.end() ? it->second : 0;
        if (frame==want) continue;
        co_await database->execSqlCoro(where.second==1
                ? "UPDATE user_units SET eqip_item_frame_id=$1 WHERE user_id=$2 AND user_unit_id=$3;"
                : "UPDATE user_units SET eqip_item_frame_id2=$1 WHERE user_id=$2 AND user_unit_id=$3;",
            want,identity.userId,where.first);
    }

    for (auto& [item,stock] : totals)
    {
        if (stock.count<0 || stock.count>UINT32_MAX)
            throw std::runtime_error("Warehouse quantity outside wire range");
        auto& copies=rows[item];
        const auto cap=static_cast<int64_t>(maxStack(item));
        int64_t have=0;
        bool first=true;
        bool importedLockedSplit=false;
        for (auto& r : copies)
        {
            if (r.unit) continue;
            if (r.count>cap)
            {
                importedLockedSplit |= r.favorite!=0;
                r.count=cap;
                co_await database->execSqlCoro("UPDATE user_warehouse_rows SET item_num=$1 WHERE instance_id=$2;",r.count,r.id);
            }
            have+=r.count;
            first=false;
        }
        if (have>stock.count)
        {
            // Keep locked rows and older surviving identities where possible.
            auto ordered=copies;
            std::sort(ordered.begin(),ordered.end(),[](const Row& a,const Row& b) {
                return a.favorite!=b.favorite ? a.favorite<b.favorite : a.id>b.id;
            });
            for (const auto& r : ordered)
            {
                if (r.unit || have<=stock.count) continue;
                const auto take=std::min(r.count,have-stock.count);
                if (!take) continue;
                co_await database->execSqlCoro(
                    "UPDATE user_warehouse_rows SET item_num=item_num-$1 WHERE instance_id=$2;",take,r.id);
                have-=take;
            }
        }
        else if (have<stock.count)
        {
            // Top up partial stacks; never recycle an exhausted sphere ID for
            // a newly granted copy (a stale sell must not sell that new copy).
            for (const auto& r : copies)
            {
                if (r.unit || r.count<=0 || r.count>=cap || have==stock.count) continue;
                const auto add=std::min(cap-r.count,stock.count-have);
                co_await database->execSqlCoro(
                    "UPDATE user_warehouse_rows SET item_num=item_num+$1 WHERE instance_id=$2;",add,r.id);
                have+=add;
            }
            while (have<stock.count)
            {
                const auto add=std::min(cap,stock.count-have);
                // An imported species-wide lock covers all copies during its
                // initial split only. Later new copies are independent.
                const auto favorite=importedLockedSplit ? 1 : (first ? stock.favorite : 0);
                co_await database->execSqlCoro(
                    "INSERT INTO user_warehouse_rows(user_id,item_id,item_num,favorite_flg,disp_order)"
                    " VALUES ($1,$2,$3,$4,$5);",identity.userId,item,add,favorite,stock.order);
                have+=add;
            }
        }
    }
    co_return;
}
}

drogon::Task<void> syncWarehouse(db::Database database, UserIdentity identity)
{
    auto transaction=std::dynamic_pointer_cast<drogon::orm::Transaction>(database);
    const bool owns=!transaction;
    if (owns) transaction=co_await database->newTransactionCoro();
    try { co_await reconcile(transaction,identity); }
    catch (...) { transaction->rollback(); throw; }
}

drogon::Task<void> releaseWarehouseSphere(db::Database database, UserIdentity identity,
    const uint32_t unitId, const int32_t slot)
{
    co_await database->execSqlCoro(
        "UPDATE user_warehouse_rows SET item_num=1,equip_unit_id=0,equip_slot=0"
        " WHERE user_id=$1 AND equip_unit_id=$2 AND equip_slot=$3;",identity.userId,unitId,slot);
}

std::optional<int64_t> warehousePlaceholderItem(const uint32_t wireId)
{
    if (wireId == 0 || wireId > static_cast<uint32_t>(INT32_MAX))
        return std::nullopt;
    const auto item = static_cast<int64_t>(INT32_MAX) - wireId;
    const auto& mst = theServer()->cache().itemMst();
    const bool known = item > 0 && std::any_of(mst.begin(), mst.end(),
        [item](const auto& row) { return row.id == item; });
    return known ? std::optional<int64_t>(item) : std::nullopt;
}

drogon::Task<std::vector<UnseenWarehouseRow>> unseenWarehouseRows(
    db::Database database, UserIdentity identity, const int64_t itemId)
{
    const auto rows = co_await database->execSqlCoro(
        "SELECT instance_id,item_num,favorite_flg,equip_unit_id FROM user_warehouse_rows"
        " WHERE user_id=$1 AND item_id=$2 AND instance_id >"
        " (SELECT COALESCE(MAX(warehouse_seen_id),0) FROM user_info WHERE id=$1)"
        " ORDER BY instance_id;", identity.userId, itemId);
    std::vector<UnseenWarehouseRow> result;
    result.reserve(rows.size());
    for (const auto& r : rows)
        result.push_back({ r["instance_id"].as<int64_t>(), r["item_num"].as<int64_t>(),
            r["favorite_flg"].as<int64_t>() != 0, r["equip_unit_id"].as<int64_t>() });
    co_return result;
}

drogon::Task<WarehouseSnapshot> loadWarehouseSnapshot(db::Database database, UserIdentity identity)
{
    auto transaction=std::dynamic_pointer_cast<drogon::orm::Transaction>(database);
    if (!transaction) transaction=co_await database->newTransactionCoro();
    try
    {
        co_await reconcile(transaction,identity);
        WarehouseSnapshot result;
        // Live rows are what the list carries; tombstones only ever mattered
        // for the seen mark and the item dictionary, which are read on their
        // own below instead of walking every tombstone (see reconcile).
        const auto rows=co_await transaction->execSqlCoro(
            "SELECT * FROM user_warehouse_rows WHERE user_id=$1 AND (item_num>0 OR equip_unit_id!=0)"
            " ORDER BY instance_id;",identity.userId);
        // Every caller sends this list as 9wjrh74P, which replaces the client's
        // own (placeholders included).  From here on the client knows every
        // live row up to the newest one by its real id; rows created after
        // this are the ones its placeholders can stand for.  The mark is the
        // newest row of any kind, as it always was.  Rolled back with the
        // reply's transaction if the reply is never sent.
        const auto newest=co_await transaction->execSqlCoro(
            "SELECT MAX(instance_id) AS newest FROM user_warehouse_rows WHERE user_id=$1;",identity.userId);
        if (!newest.empty() && !newest[0]["newest"].isNull())
            co_await transaction->execSqlCoro(
                "UPDATE user_info SET warehouse_seen_id=MAX(COALESCE(warehouse_seen_id,0),$1) WHERE id=$2;",
                newest[0]["newest"].as<int64_t>(),identity.userId);
        // Every item ever held, spent copies included (the dictionary is the
        // "obtained" record, so a tombstone still counts).
        std::set<uint32_t> dictionary;
        const auto held=co_await transaction->execSqlCoro(
            "SELECT DISTINCT item_id FROM user_warehouse_rows WHERE user_id=$1;",identity.userId);
        for (const auto& r : held)
            dictionary.insert(r["item_id"].as<uint32_t>());
        for (const auto& r : rows)
        {
            const auto item=r["item_id"].as<uint32_t>();
            UserWarehouseInfo row{};
            row.instance_id=r["instance_id"].as<uint32_t>();
            // A WORN COPY GOES OUT AS 1.  Stored as 0 (user_items excludes it),
            // but the client reads worn copies as ordinary rows: its picker
            // skips a 0-count sphere row, and equipping or unequipping never
            // changes a count -- only the unit's frame says "worn".  At 0 the
            // copy left the picker on every list, and a local unequip left a
            // row of 0 that looked like a lost sphere.  See item_num in
            // net/user.kdl.
            row.item_id=item;
            row.item_num=r["equip_unit_id"].as<int64_t>()!=0 ? 1u : r["item_num"].as<uint32_t>();
            row.disp_order=r["disp_order"].as<uint32_t>();
            result.warehouse.push_back(row);
            if (r["favorite_flg"].as<int>()!=0)
                result.favorites.push_back(::ItemFavorite{.instance_id=row.instance_id,.favorite=1});
        }
        for (const auto item : dictionary)
            result.dictionary.push_back(UserItemDictionaryInfo{.item_id=item});
        co_return result;
    }
    catch (...) { transaction->rollback(); throw; }
}
}
