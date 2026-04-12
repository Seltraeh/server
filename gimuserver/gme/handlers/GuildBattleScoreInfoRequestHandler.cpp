#include "GuildBattleScoreInfoRequestHandler.hpp"
#include "gme/response/GuildBattleScoreInfoResponse.hpp"

void Handler::GuildBattleScoreInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildBattleScoreInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
