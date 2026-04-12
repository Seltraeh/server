#include "FrontierGateInfoRequestHandler.hpp"
#include "gme/response/FrontierGateInfoResponse.hpp"

void Handler::FrontierGateInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::FrontierGateInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
