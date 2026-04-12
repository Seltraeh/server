#include "ChallengeArenaRankingRewardRequestHandler.hpp"
#include "gme/response/ChallengeArenaRankingRewardResponse.hpp"

void Handler::ChallengeArenaRankingRewardRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::ChallengeArenaRankingRewardResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
