#include "SPChallengeUserInfoRequestHandler.hpp"
#include "gme/response/SPChallengeUserInfoResponse.hpp"

void Handler::SPChallengeUserInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::SPChallengeUserInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
