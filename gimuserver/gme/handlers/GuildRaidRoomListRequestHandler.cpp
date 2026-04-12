#include "GuildRaidRoomListRequestHandler.hpp"
#include "gme/response/GuildRaidRoomListResponse.hpp"

void Handler::GuildRaidRoomListRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildRaidRoomListResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
