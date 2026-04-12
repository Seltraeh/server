#include "FGPlusGuildRankingRequestHandler.hpp"
#include "gme/response/FGPlusGuildRankingResponse.hpp"

void Handler::FGPlusGuildRankingRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::FGPlusGuildRankingResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
