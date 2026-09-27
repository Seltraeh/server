#pragma once
#include <gimuserver/gme/common/Common.hpp>

namespace gme
{
struct ExchangeOffer
{
    std::string id;
    int32_t token_id = 0;
    std::string name;
    int32_t price = 0;
    int32_t limit = 0;
    int32_t reward_type = 0;
    int32_t target_id = 0;
    int32_t quantity = 1;
    std::string source;
};

void loadExchangeArchive(const std::string& root);
const std::vector<ExchangeOffer>& eventExchangeCatalog();
std::optional<ExchangeOffer> guildExchangeOffer(int32_t id);
drogon::Task<std::vector<GuildExchangeStock>> loadGuildExchangeStock(
    db::Database database, UserIdentity identity);
drogon::Task<std::vector<EventExchangeOffer>> loadEventExchangeOffers(
    db::Database database, UserIdentity identity, int32_t token);
// Caller owns the transaction and must roll back when an error is returned.
drogon::Task<std::string> purchaseExchange(db::Database database,
    UserIdentity identity, ExchangeOffer offer, int32_t count, bool guild,
    GrantedRewards& granted);
}
