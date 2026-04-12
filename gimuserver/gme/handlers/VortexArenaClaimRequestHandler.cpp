#include "VortexArenaClaimRequestHandler.hpp"
#include "gme/response/VortexArenaClaimResponse.hpp"

void Handler::VortexArenaClaimRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::VortexArenaClaimResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
