#include "GetScenarioPlayingInfoRequestHandler.hpp"

void Handler::GetScenarioPlayingInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
