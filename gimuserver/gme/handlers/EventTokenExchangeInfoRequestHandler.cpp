#include "EventTokenExchangeInfoRequestHandler.hpp"
#include "gme/response/EventTokenExchangeInfoResponse.hpp"

void Handler::EventTokenExchangeInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::EventTokenExchangeInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
