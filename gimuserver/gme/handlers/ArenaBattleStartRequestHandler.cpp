#include "ArenaBattleStartRequestHandler.hpp"

void Handler::ArenaBattleStartRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
