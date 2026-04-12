#include "GuildPreviousRankingRequestHandler.hpp"
#include "gme/response/GuildPreviousRankingResponse.hpp"

void Handler::GuildPreviousRankingRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildPreviousRankingResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
