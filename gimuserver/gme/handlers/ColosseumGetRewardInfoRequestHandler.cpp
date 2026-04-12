#include "ColosseumGetRewardInfoRequestHandler.hpp"

void Handler::ColosseumGetRewardInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
