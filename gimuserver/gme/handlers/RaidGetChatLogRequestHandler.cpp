#include "RaidGetChatLogRequestHandler.hpp"
#include "gme/response/RaidGetChatLogResponse.hpp"

void Handler::RaidGetChatLogRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::RaidGetChatLogResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
