#include "SGChatLogInfoListRequestHandler.hpp"
#include "gme/response/SGChatLogInfoListResponse.hpp"

void Handler::SGChatLogInfoListRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::SGChatLogInfoListResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
