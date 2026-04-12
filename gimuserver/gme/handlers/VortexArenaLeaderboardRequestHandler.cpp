#include "VortexArenaLeaderboardRequestHandler.hpp"
#include "gme/response/VortexArenaLeaderboardResponse.hpp"

void Handler::VortexArenaLeaderboardRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::VortexArenaLeaderboardResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
