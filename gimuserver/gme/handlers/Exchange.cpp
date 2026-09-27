#include "App.hpp"
#include "Handlers.hpp"
#include <gimuserver/gme/common/Exchange.hpp>
#include <gimuserver/gme/common/EventTokens.hpp>
#include <gimuserver/gme/common/Guilds.hpp>

HANDLEF(EventExchangeInfo)
{
    (void)session;
    EventExchangeReq req{};
    if (glz::read<glz::opts{.error_on_unknown_keys=false}>(req, json))
        co_return HandleResult::error("Invalid exchange request");
    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    if (req.tokens.size() != 1) co_return HandleResult::error("Select one event currency");
    int32_t token = 0;
    try { token = std::stoi(req.tokens.front().token_id); }
    catch (const std::exception&) { co_return HandleResult::error("Invalid event currency"); }
    ExchangeResp resp{};
    resp.offers = co_await gme::loadEventExchangeOffers(theDb(), identity, token);
    resp.event_token_info = co_await gme::loadEventTokens(theDb(), identity);
    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

HANDLEF(EventExchangePurchase)
{
    (void)session;
    EventExchangePurchaseReq req{};
    if (glz::read<glz::opts{.error_on_unknown_keys=false}>(req, json) || req.nodes.empty() || req.nodes.size() > 50)
        co_return HandleResult::error("Invalid exchange purchase");
    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    ExchangeResp resp{};
    std::vector<EventExchangeResult> results;
    int32_t token = 0;
    {
        auto transaction = co_await theDb()->newTransactionCoro();
        try
        {
            gme::GrantedRewards granted;
            for (const auto& node : req.nodes)
            {
                const auto& catalog = gme::eventExchangeCatalog();
                const auto it = std::find_if(catalog.begin(), catalog.end(),
                    [&](const auto& offer) { return offer.id == node.id && std::to_string(offer.token_id) == node.token_id; });
                if (it == catalog.end() || (token && token != it->token_id))
                {
                    transaction->rollback();
                    co_return HandleResult::error("Unknown offer or mismatched event currency");
                }
                token = it->token_id;
                const auto error = co_await gme::purchaseExchange(transaction, identity, *it, node.count, false, granted);
                if (!error.empty())
                {
                    transaction->rollback();
                    co_return HandleResult::error(error);
                }
                results.push_back({node.id, 1, ""});
            }
            resp.results = std::move(results);
            resp.offers = co_await gme::loadEventExchangeOffers(transaction, identity, token);
            resp.event_token_info = co_await gme::loadEventTokens(transaction, identity);
            co_await gme::emitGrantedRewards(transaction, identity, granted, resp);
        }
        catch (...)
        {
            transaction->rollback();
            throw;
        }
    }
    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

HANDLEF(GuildTrade)
{
    (void)session;
    GuildTradeReq req{};
    if (glz::read<glz::opts{.error_on_unknown_keys=false}>(req, json) || req.nodes.size() != 1)
        co_return HandleResult::error("Invalid Guild exchange purchase");
    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const auto offer = gme::guildExchangeOffer(req.nodes.front().id);
    if (!offer) co_return HandleResult::error("Unknown Guild exchange offer");
    ExchangeResp resp{};
    {
        auto transaction = co_await theDb()->newTransactionCoro();
        try
        {
            if (!(co_await gme::loadGuild(transaction, identity)))
            {
                transaction->rollback();
                co_return HandleResult::error("Join a guild before using its exchange");
            }
            gme::GrantedRewards granted;
            const auto error = co_await gme::purchaseExchange(transaction, identity, *offer,
                req.nodes.front().count, true, granted);
            if (!error.empty())
            {
                transaction->rollback();
                co_return HandleResult::error(error);
            }
            resp.guild_stock = co_await gme::loadGuildExchangeStock(transaction, identity);
            resp.guild_members = co_await gme::guildRoster(transaction, identity);
            co_await gme::emitGrantedRewards(transaction, identity, granted, resp);
        }
        catch (...)
        {
            transaction->rollback();
            throw;
        }
    }
    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
