#include "ChallengeRankingRequestHandler.hpp"
#include "gme/response/ChallengeRankingResponse.hpp"

void Handler::ChallengeRankingRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::ChallengeRankingResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
