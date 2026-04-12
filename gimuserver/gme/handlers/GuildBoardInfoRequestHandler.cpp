#include "GuildBoardInfoRequestHandler.hpp"
#include "gme/response/GuildBoardInfoResponse.hpp"

void Handler::GuildBoardInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildBoardInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
