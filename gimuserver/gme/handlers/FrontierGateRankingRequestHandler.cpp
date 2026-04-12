#include "FrontierGateRankingRequestHandler.hpp"
#include "gme/response/FrontierGateRankingResponse.hpp"

void Handler::FrontierGateRankingRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::FrontierGateRankingResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
