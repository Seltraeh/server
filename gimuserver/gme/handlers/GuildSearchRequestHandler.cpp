#include "GuildSearchRequestHandler.hpp"
#include "gme/response/GuildSearchResponse.hpp"

void Handler::GuildSearchRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildSearchResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
