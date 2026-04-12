#include "GuildMemberGuildInfoRequestHandler.hpp"
#include "gme/response/GuildMemberGuildInfoResponse.hpp"

void Handler::GuildMemberGuildInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildMemberGuildInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
