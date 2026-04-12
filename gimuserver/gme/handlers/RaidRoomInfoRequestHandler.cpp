#include "RaidRoomInfoRequestHandler.hpp"
#include "gme/response/RaidRoomInfoResponse.hpp"

void Handler::RaidRoomInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::RaidRoomInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
