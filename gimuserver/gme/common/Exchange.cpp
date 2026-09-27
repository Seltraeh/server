#include "App.hpp"
#include "Exchange.hpp"
#include <gimuserver/utils/JsonFile.hpp>
#include <gimuserver/archive/UnitArchiver.hpp>
#include <map>
#include <set>

namespace gme
{
namespace
{
std::vector<ExchangeOffer> eventOffers;
bool validReward(const ExchangeOffer& offer)
{
    if (offer.price <= 0 || offer.limit <= 0 || offer.quantity <= 0) return false;
    if (offer.reward_type == 6)
        return UnitArchiver::instance().lookup(offer.target_id).has_value();
    if (offer.reward_type != 4 && offer.reward_type != 5 && offer.reward_type != 7)
        return false;
    for (const auto& item : theServer()->cache().itemMst())
        if (item.id == offer.target_id) return true;
    return false;
}
}

void loadExchangeArchive(const std::string& root)
{
    eventOffers = LoadJson<std::vector<ExchangeOffer>>(root, "event_exchange.json");
    std::set<std::string> ids;
    for (const auto& offer : eventOffers)
        if (offer.id.empty() || !ids.insert(offer.id).second || offer.token_id <= 0 || !validReward(offer))
            throw std::runtime_error("Invalid event exchange offer: " + offer.id);
    LOG_INFO << "Event exchange: " << eventOffers.size() << " offers";
}

const std::vector<ExchangeOffer>& eventExchangeCatalog() { return eventOffers; }

std::optional<ExchangeOffer> guildExchangeOffer(int32_t id)
{
    for (const auto& row : theServer()->cache().guildPointExchangeMst())
    {
        if (row.id != id || row.reward_info.size() < 3) continue;
        ExchangeOffer offer{};
        offer.id = std::to_string(id);
        offer.name = row.name;
        offer.price = row.price;
        offer.limit = row.limit_count;
        try
        {
            offer.reward_type = std::stoi(row.reward_info[0]);
            offer.target_id = std::stoi(row.reward_info[1]);
            offer.quantity = std::stoi(row.reward_info[2]);
        }
        catch (const std::exception&) { return std::nullopt; }
        if (validReward(offer)) return offer;
        return std::nullopt;
    }
    return std::nullopt;
}

drogon::Task<std::vector<GuildExchangeStock>> loadGuildExchangeStock(
    db::Database database, UserIdentity identity)
{
    std::map<std::string, int32_t> counts;
    for (const auto& row : co_await database->execSqlCoro(
        "SELECT offer_id, count FROM user_exchange_purchases WHERE user_id=$1 AND shop='guild';", identity.userId))
        counts[row["offer_id"].as<std::string>()] = row["count"].as<int32_t>();
    std::vector<GuildExchangeStock> result;
    for (const auto& offer : theServer()->cache().guildPointExchangeMst())
        if (guildExchangeOffer(offer.id))
            result.push_back({offer.id, counts[std::to_string(offer.id)], -1, 0});
    co_return result;
}

drogon::Task<std::vector<EventExchangeOffer>> loadEventExchangeOffers(
    db::Database database, UserIdentity identity, int32_t token)
{
    std::map<std::string, int32_t> counts;
    for (const auto& row : co_await database->execSqlCoro(
        "SELECT offer_id, count FROM user_exchange_purchases WHERE user_id=$1 AND shop='event';", identity.userId))
        counts[row["offer_id"].as<std::string>()] = row["count"].as<int32_t>();
    std::vector<EventExchangeOffer> result;
    for (const auto& offer : eventOffers)
    {
        if (offer.token_id != token) continue;
        EventExchangeOffer row{};
        row.id = offer.id;
        row.name = offer.name;
        row.order = static_cast<int32_t>(result.size() + 1);
        row.price = offer.price;
        row.remaining = std::max(0, offer.limit - counts[offer.id]);
        row.reward = std::to_string(offer.reward_type) + ':' + std::to_string(offer.target_id)
            + ':' + std::to_string(offer.quantity) + ":0:0";
        result.push_back(std::move(row));
    }
    co_return result;
}

drogon::Task<std::string> purchaseExchange(db::Database database,
    UserIdentity identity, ExchangeOffer offer, int32_t count, bool guild,
    GrantedRewards& granted)
{
    if (count <= 0 || count > offer.limit || !validReward(offer))
        co_return "Invalid purchase quantity or reward";
    const auto quantity = static_cast<int64_t>(count) * offer.quantity;
    if (quantity > 100000) co_return "Purchase quantity too large";
    const auto cost = static_cast<int64_t>(count) * offer.price;
    const auto shop = guild ? std::string("guild") : std::string("event");
    const auto bought = co_await database->execSqlCoro(
        "INSERT INTO user_exchange_purchases(user_id,shop,offer_id,count) VALUES($1,$2,$3,$4)"
        " ON CONFLICT(user_id,shop,offer_id) DO UPDATE SET count=count+$4"
        " WHERE count+$4 <= $5 RETURNING count;",
        identity.userId, shop, offer.id, count, offer.limit);
    if (bought.empty()) co_return "Purchase limit reached";
    if (guild)
    {
        const auto paid = co_await database->execSqlCoro(
            "UPDATE user_info SET guild_tokens=guild_tokens-$1 WHERE id=$2 AND guild_tokens >= $1 RETURNING guild_tokens;",
            cost, identity.userId);
        if (paid.empty()) co_return "Not enough Guild Tokens";
    }
    else
    {
        const auto paid = co_await database->execSqlCoro(
            "UPDATE user_event_tokens SET count=count-$1 WHERE user_id=$2 AND token_id=$3 AND count >= $1 RETURNING count;",
            cost, identity.userId, std::to_string(offer.token_id));
        if (paid.empty()) co_return "Not enough event tokens";
    }
    if (offer.reward_type == 6)
    {
        for (int64_t i = 0; i < quantity; ++i)
        {
            const auto unit = fromArchivedUnit(offer.target_id, UnitArchiver::getRandomType());
            if (!unit) co_return "Missing unit archive";
            const auto added = (co_await addUserUnit(database, identity, *unit)).nonEmpty();
            granted.userUnitIds.push_back(added.user_unit_id);
        }
    }
    else
    {
        (co_await addUserItem(database, identity, offer.target_id, static_cast<uint32_t>(quantity))).nonEmpty();
        granted.items = true;
    }
    co_return std::string{};
}
}
