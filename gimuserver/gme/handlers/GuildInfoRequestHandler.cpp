#include "GuildInfoRequestHandler.hpp"
#include "gme/response/GuildInfoResponse.hpp"

void Handler::GuildInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
