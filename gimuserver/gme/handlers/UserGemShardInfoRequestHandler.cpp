#include "UserGemShardInfoRequestHandler.hpp"
#include "gme/response/UserGemShardInfoResponse.hpp"

void Handler::UserGemShardInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::UserGemShardInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
