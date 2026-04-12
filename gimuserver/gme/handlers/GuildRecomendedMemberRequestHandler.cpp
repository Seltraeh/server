#include "GuildRecomendedMemberRequestHandler.hpp"
#include "gme/response/GuildRecomendedMemberResponse.hpp"

void Handler::GuildRecomendedMemberRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildRecomendedMemberResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
