#include "GuildRaidBattleLogListRequestHandler.hpp"
#include "gme/response/GuildRaidBattleLogListResponse.hpp"

void Handler::GuildRaidBattleLogListRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildRaidBattleLogListResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
