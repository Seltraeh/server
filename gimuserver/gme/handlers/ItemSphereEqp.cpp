#include "App.hpp"
#include "Handlers.hpp"
#include <gimuserver/gme/common/Common.hpp>
#include <array>
#include <charconv>
#include <optional>
#include <set>
#include <string_view>

namespace
{
/// Splits on one delimiter, KEEPING empty pieces: an unequip arrives as
/// "0:0:1000::", which is five fields, the last two empty.  getline drops the
/// trailing one and reads it as four.
std::vector<std::string_view> splitKeepEmpty(const std::string_view in, const char sep)
{
    std::vector<std::string_view> out;
    size_t start = 0;
    while (true)
    {
        const auto at = in.find(sep, start);
        if (at == std::string_view::npos)
        {
            out.push_back(in.substr(start));
            return out;
        }
        out.push_back(in.substr(start, at - start));
        start = at + 1;
    }
}

/// One field of a segment.  "" is 0 -- the unit's string for an id it never
/// held -- and the sign is kept, because frame2 is "-1" on every unit without
/// a Sphere Frog.  Anything else non-numeric is malformed.
std::optional<int64_t> parseField(const std::string_view token)
{
    if (token.empty())
        return 0;
    int64_t value = 0;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error != std::errc{} || end != token.data() + token.size())
        return std::nullopt;
    return value;
}
}

// wx1ZLFj9.a2utCvs8 = frame1:frame2:unit:item1:item2[,...] -- one segment per
// unit the screen changed, every field that unit's own string (see the field
// doc in net/items.kdl).  A frame names the warehouse row of the copy in the
// slot: a real row id, an INT32_MAX - item placeholder for a copy the client
// crafted itself, 0 for an empty slot, or -1 for the second slot of a unit
// without a Sphere Frog; an item is an ItemMst id, or 0/"" for none.
//
// Retain copy identity (including its favorite flag) while it is equipped.
// The entire batch is atomic; return all changed slots before assigning new
// ones so moving a sphere between two units works in either request order.
HANDLEF(ItemSphereEqp)
{
    (void)session;
    ItemSphereEqpReq req{};
    if (const auto ec=glz::read<glz::opts{.error_on_unknown_keys=false}>(req,json); ec)
    {
        LOG_WARN << "ItemSphereEqp: unreadable request: " << glz::format_error(ec, json);
        co_return HandleResult::success("{}");
    }
    struct Change { uint32_t unit,slot,item,wire; int32_t frame; bool hasSlot2; };
    // {frame1, frame2, unit, item1, item2}.  Frames are folded to 0 when
    // negative: -1 is "this slot does not exist", which names no copy.
    std::vector<std::array<uint32_t,5>> ops;
    std::set<uint32_t> namedUnits;
    for (const auto& node:req.nodes)
    {
        for (const auto segment:splitKeepEmpty(node.packed,','))
        {
            // An empty change list is sent as an empty string.
            if (segment.empty()) continue;
            const auto fields=splitKeepEmpty(segment,':');
            std::array<uint32_t,5> values{};
            bool valid=fields.size()==values.size();
            for (size_t n=0; valid && n<values.size(); ++n)
            {
                const auto value=parseField(fields[n]);
                const bool frame=n<2;
                valid=value && *value<=static_cast<int64_t>(UINT32_MAX) && (frame || *value>=0);
                if (valid) values[n]=static_cast<uint32_t>(std::max<int64_t>(*value,0));
            }
            // Refused whole and LOGGED.  This used to return silently, and a
            // silent refusal is invisible to the player: the client has
            // already drawn the change, and the next HomeInfo unit rebuild
            // takes it back.
            if (!valid || values[2]==0 || !namedUnits.insert(values[2]).second)
            {
                LOG_WARN << "ItemSphereEqp: refused malformed batch \"" << node.packed
                         << "\" at segment \"" << std::string(segment) << "\"";
                co_return HandleResult::success("{}");
            }
            ops.push_back(values);
        }
    }
    const auto identity=(co_await gme::getUserIdentity(theDb(),req.login_info)).nonEmpty();
    auto transaction=co_await theDb()->newTransactionCoro();
    try
    {
        co_await gme::syncWarehouse(transaction,identity);
        std::vector<Change> changes;
        for (const auto& op:ops)
        {
            const auto unit=op[2];
            const auto owned=co_await transaction->execSqlCoro(
                "SELECT eqip_item_id,eqip_item_id2,sphere_ext FROM user_units WHERE user_id=$1 AND user_unit_id=$2;",
                identity.userId,unit);
            if (owned.empty()) throw std::invalid_argument("unit not owned");
            const bool slot2=owned[0]["sphere_ext"].as<int>()!=0;
            for (uint32_t slot=1;slot<=2;++slot)
            {
                const auto item=op[slot+2], wire=op[slot-1];
                const auto old=owned[0][slot==1 ? "eqip_item_id" : "eqip_item_id2"].as<uint32_t>();
                if (slot==2 && !slot2 && item) throw std::invalid_argument("second sphere slot locked");
                // An emptied slot's frame; an equipped one gets its copy's
                // row below, once that row is chosen.
                int32_t frame=slot==2 && !slot2 ? gme::kNoSecondSphereSlot : 0;
                if (item)
                {
                    const auto& mst=theServer()->cache().itemMst();
                    const auto it=std::find_if(mst.begin(),mst.end(),[item](const auto& r){return r.id==item;});
                    if (it==mst.end() || (it->item_type!=3 && it->item_type!=6))
                        throw std::invalid_argument("item is not a sphere");
                }
                const auto prior=co_await transaction->execSqlCoro(
                    "SELECT instance_id FROM user_warehouse_rows WHERE user_id=$1 AND equip_unit_id=$2 AND equip_slot=$3;",
                    identity.userId,unit,slot);
                // Unchanged: the same copy, or a copy the client equipped
                // before it was sent the real id and still names by the
                // placeholder (gme::warehousePlaceholderItem).
                if (old==item && (!wire || (!prior.empty() && prior[0]["instance_id"].as<uint32_t>()==wire)
                        || gme::warehousePlaceholderItem(wire)==static_cast<int64_t>(item)))
                    continue;
                changes.push_back({unit,slot,item,wire,frame,slot2});
                if (old)
                {
                    co_await gme::releaseWarehouseSphere(transaction,identity,unit,slot);
                    co_await gme::addUserItem(transaction,identity,old,1);
                }
            }
        }
        for (const auto& change:changes)
        {
            auto frame=change.frame;
            if (change.item)
            {
                // A zero warehouse id is the older species-only request form.
                // Nonzero ids must identify the actual selected owned copy.
                const auto chosen=co_await transaction->execSqlCoro(
                    "SELECT instance_id FROM user_warehouse_rows WHERE user_id=$1 AND item_id=$2"
                    " AND item_num>0 AND equip_unit_id=0 AND ($3=0 OR instance_id=$3)"
                    " ORDER BY instance_id LIMIT 1;",identity.userId,change.item,change.wire);
                int64_t row=chosen.empty() ? 0 : chosen[0]["instance_id"].as<int64_t>();
                if (!row && change.wire
                    && gme::warehousePlaceholderItem(change.wire)==static_cast<int64_t>(change.item))
                {
                    // Not a real row: a copy the client added itself (a craft)
                    // and equipped before it was sent the real id.  Take the
                    // oldest free copy it has not been sent -- an unlocked one
                    // first, so a lock stays on a copy still in storage, where
                    // it is what keeps the copy from being sold.
                    const auto real=co_await transaction->execSqlCoro(
                        "SELECT 1 FROM user_warehouse_rows WHERE user_id=$1 AND instance_id=$2;",
                        identity.userId,change.wire);
                    if (real.empty())
                    {
                        const auto unseen=co_await gme::unseenWarehouseRows(transaction,identity,change.item);
                        for (const bool locked:{false,true})
                        {
                            for (const auto& r:unseen)
                            {
                                if (r.count<=0 || r.equipUnitId!=0 || r.favorite!=locked) continue;
                                row=r.instanceId;
                                break;
                            }
                            if (row) break;
                        }
                    }
                }
                if (!row) throw std::invalid_argument("sphere copy unavailable");
                const auto spent=co_await transaction->execSqlCoro(
                    "UPDATE user_items SET item_num=item_num-1 WHERE user_id=$1 AND item_id=$2"
                    " AND item_num>0 RETURNING item_num;",identity.userId,change.item);
                if (spent.empty()) throw std::invalid_argument("sphere stock unavailable");
                co_await transaction->execSqlCoro(
                    "UPDATE user_warehouse_rows SET item_num=0,equip_unit_id=$1,equip_slot=$2 WHERE instance_id=$3;",
                    change.unit,change.slot,row);
                // THE FRAME IS THE COPY'S ROW, not its sphere_type: the
                // client's "which unit wears this row" map is keyed on it
                // (UserUnitInfoList::updateSphereEquipList), and the next batch
                // that carries this slot unchanged names the copy by it.
                frame=static_cast<int32_t>(row);
            }
            const std::string columns=change.slot==1 ? "eqip_item_id=$1,eqip_item_frame_id=$2" : "eqip_item_id2=$1,eqip_item_frame_id2=$2";
            co_await transaction->execSqlCoro(
                "UPDATE user_units SET "+columns+" WHERE user_id=$3 AND user_unit_id=$4;",
                change.item,frame,identity.userId,change.unit);
        }
    }
    catch (const std::invalid_argument& e)
    {
        transaction->rollback();
        std::string batch;
        for (const auto& node:req.nodes) batch+=(batch.empty() ? "" : "|")+node.packed;
        LOG_WARN << "ItemSphereEqp: refused batch \"" << batch << "\": " << e.what();
        co_return HandleResult::success("{}");
    }
    catch (...) { transaction->rollback(); throw; }
    co_return HandleResult::success("{}");
}
