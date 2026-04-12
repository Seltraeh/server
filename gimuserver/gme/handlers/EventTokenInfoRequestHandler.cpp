#include "EventTokenInfoRequestHandler.hpp"
#include "gme/response/EventTokenInfoResponse.hpp"

void Handler::EventTokenInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::EventTokenInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
