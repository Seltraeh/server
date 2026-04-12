#include "ChallengeArenaRankingRequestHandler.hpp"
#include "gme/response/ChallengeArenaRankingResponse.hpp"

void Handler::ChallengeArenaRankingRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::ChallengeArenaRankingResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
