#include "DungeonKeyUseRequestHandler.hpp"

void Handler::DungeonKeyUseRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
