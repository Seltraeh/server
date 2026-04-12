#include "GuildRankingDetailRequestHandler.hpp"
#include "gme/response/GuildRankingDetailResponse.hpp"

void Handler::GuildRankingDetailRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::GuildRankingDetailResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
