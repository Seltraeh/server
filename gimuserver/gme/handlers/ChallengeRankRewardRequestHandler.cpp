#include "ChallengeRankRewardRequestHandler.hpp"
#include "gme/response/ChallengeRankRewardResponse.hpp"

void Handler::ChallengeRankRewardRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::ChallengeRankRewardResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
