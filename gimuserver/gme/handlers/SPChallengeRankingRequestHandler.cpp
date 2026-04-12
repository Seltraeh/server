#include "SPChallengeRankingRequestHandler.hpp"
#include "gme/response/SPChallengeRankingResponse.hpp"

void Handler::SPChallengeRankingRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::SPChallengeRankingResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
