#include "UserTournamentInfoRequestHandler.hpp"
#include "gme/response/UserTournamentInfoResponse.hpp"

void Handler::UserTournamentInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::UserTournamentInfoResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
