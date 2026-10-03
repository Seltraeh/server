#include "App.hpp"
#include "Handlers.hpp"
#include <gimuserver/gme/common/Common.hpp>
#include <map>
#include <utility>
#include <vector>

// Exact persistent warehouse row ids; the empty acknowledgement is the
// existing client contract. Stock, Zel and counters commit together.
HANDLEF(ItemSell)
{
    (void)session;
    ItemSellReq req{};
    if (const auto ec=glz::read<glz::opts{.error_on_unknown_keys=false}>(req,json); ec)
        co_return HandleResult::success("{}");
    const auto identity=(co_await gme::getUserIdentity(theDb(),req.login_info)).nonEmpty();
    auto transaction=co_await theDb()->newTransactionCoro();
    try
    {
        co_await gme::syncWarehouse(transaction,identity);
        // Ascending ids: real rows are settled before any placeholder
        // (INT_MAX - itemId) is resolved among the rows that are left.
        std::map<uint32_t,uint64_t> counts;
        for (const auto& e:req.entries) counts[e.instance_id]+=e.item_num;
        int64_t credit=0;
        for (const auto& [id,count]:counts)
        {
            const auto refuse=[&](const char* why) {
                transaction->rollback();
                LOG_WARN << "ItemSell: " << why << " row " << id;
            };
            // (row, quantity) pairs this entry sells from.
            std::vector<std::pair<int64_t,int64_t>> takes;
            int64_t item=0;
            const auto rows=co_await transaction->execSqlCoro(
                "SELECT item_id,item_num,favorite_flg,equip_unit_id FROM user_warehouse_rows"
                " WHERE user_id=$1 AND instance_id=$2;",identity.userId,id);
            if (!rows.empty())
            {
                if (count==0 || count>static_cast<uint64_t>(rows[0]["item_num"].as<int64_t>())
                    || rows[0]["favorite_flg"].as<int>()!=0 || rows[0]["equip_unit_id"].as<int64_t>()!=0)
                {
                    refuse("invalid, locked or unavailable");
                    co_return HandleResult::success("{}");
                }
                item=rows[0]["item_id"].as<int64_t>();
                takes.emplace_back(static_cast<int64_t>(id),static_cast<int64_t>(count));
            }
            else if (const auto placeholder=gme::warehousePlaceholderItem(id); placeholder && count>0)
            {
                // A copy the client added itself (a craft, a present) and still
                // names by its placeholder: sell from the rows it has not been
                // sent, never a locked or equipped one -- its sell list offers
                // neither.  Short of stock is the same refusal as a bad row.
                item=*placeholder;
                auto left=static_cast<int64_t>(count);
                const auto unseen=co_await gme::unseenWarehouseRows(transaction,identity,item);
                for (const auto& r:unseen)
                {
                    if (r.favorite || r.equipUnitId!=0 || r.count<=0) continue;
                    const auto take=std::min(left,r.count);
                    takes.emplace_back(r.instanceId,take);
                    left-=take;
                    if (left==0) break;
                }
                if (left!=0)
                {
                    refuse("placeholder has no unsent copies for");
                    co_return HandleResult::success("{}");
                }
            }
            else
            {
                refuse("invalid, locked or unavailable");
                co_return HandleResult::success("{}");
            }
            const auto& mst=theServer()->cache().itemMst();
            const auto it=std::find_if(mst.begin(),mst.end(),[item](const auto& r){return r.id==item;});
            if (it==mst.end() || it->sell_price<0)
                throw std::runtime_error("ItemSell: invalid item master");
            const auto quantity=static_cast<int64_t>(count);
            const auto spent=co_await transaction->execSqlCoro(
                "UPDATE user_items SET item_num=item_num-$1 WHERE user_id=$2 AND item_id=$3"
                " AND item_num >= $1 RETURNING item_num;",quantity,identity.userId,item);
            if (spent.empty()) throw std::runtime_error("ItemSell: stock mismatch");
            for (const auto& [row,take]:takes)
                co_await transaction->execSqlCoro(
                    "UPDATE user_warehouse_rows SET item_num=item_num-$1 WHERE instance_id=$2;",take,row);
            credit+=quantity*it->sell_price;
        }
        if (credit>0)
        {
            co_await transaction->execSqlCoro(
                "UPDATE user_info SET zel=MIN(zel+$1,99999999) WHERE id=$2;",credit,identity.userId);
            co_await gme::bumpArchiveCounters(transaction,identity,{{"zel_item_sale",credit}});
        }
    }
    catch (...) { transaction->rollback(); throw; }
    co_return HandleResult::success("{}");
}
