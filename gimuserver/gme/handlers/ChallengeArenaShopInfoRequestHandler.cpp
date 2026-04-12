#include "ChallengeArenaShopInfoRequestHandler.hpp"
#include "gme/response/ChallengeArenaShopInfoResponse.hpp"

void Handler::ChallengeArenaShopInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::ChallengeArenaShopInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
