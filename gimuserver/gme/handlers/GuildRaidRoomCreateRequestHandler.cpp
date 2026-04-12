#include "GuildRaidRoomCreateRequestHandler.hpp"

void Handler::GuildRaidRoomCreateRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
