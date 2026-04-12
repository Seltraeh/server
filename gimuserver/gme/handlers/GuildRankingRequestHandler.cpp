#include "GuildRankingRequestHandler.hpp"
#include "gme/response/GuildRankingResponse.hpp"

void Handler::GuildRankingRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildRankingResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
