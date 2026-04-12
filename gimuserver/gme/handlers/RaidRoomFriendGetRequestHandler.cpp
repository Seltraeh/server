#include "RaidRoomFriendGetRequestHandler.hpp"
#include "gme/response/RaidRoomFriendGetResponse.hpp"

void Handler::RaidRoomFriendGetRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::RaidRoomFriendGetResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
