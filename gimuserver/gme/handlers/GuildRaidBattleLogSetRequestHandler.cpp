#include "GuildRaidBattleLogSetRequestHandler.hpp"

void Handler::GuildRaidBattleLogSetRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
