#include "App.hpp"
#include "Handlers.hpp"
#include <gimuserver/gme/common/Common.hpp>
#include <map>

// The request is the complete favorite set. Persistent rows let identical
// sphere copies retain independent locks across updates and server restarts.
//
// WHAT THE CLIENT CAN SAY, AND WHAT IT SHOWS AFTERWARDS (Windows client,
// BraveFrontier.Windows.exe; the arm64 build agrees):
//   * ItemDetailScene::touchEnded sets the flag on the row being viewed and
//     QUEUES an ItemFavoriteRequest (ConnectRequestList, deduplicated by
//     request type); its body is built when StepScene sends the queue.
//   * The builder (Windows RVA 0x4B4060, arm64 createBody @0x13A63B4) takes
//     one key per ROW (getAllKeys, RVA 0x5C09C0 -- duplicates kept), looks
//     each up with getObjectAtItemIndex (RVA 0x20D090 -- the FIRST row with
//     that id) and sends n6E8iMf3 = that id only when that row is locked
//     ([row+0x40] == 1).
//   * The reply's VSRPkdId replaces ItemFavoriteInfoList, and
//     UserWarehouseInfoList::setFavorite (@0x12BCA34) then sets EVERY row's
//     flag to "is my id in that list".
// Every copy the client crafted itself (GameUtils::incWarehouseItem: one row
// per copy for a max-stack-1 sphere) carries the one placeholder id
// INT32_MAX - item_id.  So for a species with k such rows the request says
// either "placeholder x k" (the FIRST of them is locked) or nothing (the lock,
// if any, is on the 2nd..kth row and is never sent), and after the reply the
// client shows all k locked or all k unlocked.  The server does the same with
// the k copies the client has not been sent yet, so the two never disagree.
// A lock put on the 2nd..kth provisional copy is lost at that point -- the
// request cannot carry it -- and the reply shows it gone; it sticks once the
// next warehouse list has given each copy its own id.
HANDLEF(ItemFavorite)
{
    (void)session;
    ItemFavoriteReq req{};
    if (const auto ec=glz::read<glz::opts{.error_on_unknown_keys=false}>(req,json); ec)
        co_return HandleResult::success("{}");
    const auto identity=(co_await gme::getUserIdentity(theDb(),req.login_info)).nonEmpty();
    auto transaction=co_await theDb()->newTransactionCoro();
    std::string buffer;
    try
    {
        co_await gme::syncWarehouse(transaction,identity);
        co_await transaction->execSqlCoro(
            "UPDATE user_warehouse_rows SET favorite_flg=0 WHERE user_id=$1;",identity.userId);
        ItemFavoriteResp resp{};
        // AN ENTRY THAT CANNOT BE MAPPED REFUSES THE WHOLE SET, and every lock
        // stays as it was: an id that is not this player's live copy, or a
        // placeholder with no unsent copy behind it (the client is still holding
        // rows from before a warehouse list it never received -- a lost reply --
        // and those rows are copies the server now knows by their real ids).
        // Losing a lock is the one outcome worse than a stale icon.
        std::map<int64_t,std::pair<uint32_t,int>> placeholders;   // item -> (wire id, nodes)
        for (const auto& e:req.entries)
        {
            if (!e.favorite) continue;
            const auto updated=co_await transaction->execSqlCoro(
                "UPDATE user_warehouse_rows SET favorite_flg=1 WHERE user_id=$1 AND instance_id=$2"
                " AND (item_num>0 OR equip_unit_id!=0) RETURNING instance_id;",identity.userId,e.instance_id);
            if (!updated.empty())
            {
                resp.entries.push_back(::ItemFavorite{.instance_id=e.instance_id,.favorite=1});
                continue;
            }
            if (const auto placeholder=gme::warehousePlaceholderItem(e.instance_id))
            {
                auto& [wire,nodes]=placeholders[*placeholder];
                wire=e.instance_id;
                ++nodes;
                continue;
            }
            transaction->rollback();
            LOG_WARN << "ItemFavorite: unknown or exhausted row " << e.instance_id << "; the set is refused unchanged";
            co_return HandleResult::success("{}");
        }
        for (const auto& [item,entry]:placeholders)
        {
            // Lock as many of the copies the client has not been sent as it
            // holds placeholder rows -- the oldest first, and ones still in
            // storage before equipped ones, since storage is where a lock keeps
            // a copy from being sold.
            const auto [wire,nodes]=entry;
            int locked=0;
            const auto unseen=co_await gme::unseenWarehouseRows(transaction,identity,item);
            for (const bool equipped:{false,true})
                for (const auto& r:unseen)
                {
                    if (locked>=nodes) break;
                    if (equipped ? r.equipUnitId==0 : (r.count<=0 || r.equipUnitId!=0)) continue;
                    co_await transaction->execSqlCoro(
                        "UPDATE user_warehouse_rows SET favorite_flg=1 WHERE instance_id=$1;",r.instanceId);
                    ++locked;
                }
            if (locked==0)
            {
                transaction->rollback();
                LOG_WARN << "ItemFavorite: " << identity.userId << " locked new copies of item " << item
                    << " but none exist that the client has not been sent; the set is refused unchanged";
                co_return HandleResult::success("{}");
            }
            if (locked<nodes)
                LOG_WARN << "ItemFavorite: " << identity.userId << " locked " << nodes << " new copies of item "
                    << item << " but only " << locked << " exist that the client has not been sent";
            // Echo the client's own id: its ItemFavoriteInfoList is keyed by the
            // ids it holds, and a placeholder's real rows are not among them yet.
            for (int n=0;n<locked;++n)
                resp.entries.push_back(::ItemFavorite{.instance_id=wire,.favorite=1});
        }
        // Keep the old species-level flag conservative for legacy consumers;
        // row-aware consumers use the independent flags above.
        co_await transaction->execSqlCoro(
            "UPDATE user_items SET favorite_flg=EXISTS(SELECT 1 FROM user_warehouse_rows w"
            " WHERE w.user_id=user_items.user_id AND w.item_id=user_items.item_id"
            " AND w.favorite_flg!=0 AND (w.item_num>0 OR w.equip_unit_id!=0)) WHERE user_id=$1;",identity.userId);
        if (const auto ec=glz::write_json(resp,buffer); ec)
            throw std::runtime_error("ItemFavorite: serialization failed");
    }
    catch (...) { transaction->rollback(); throw; }
    co_return HandleResult::success(buffer);
}
