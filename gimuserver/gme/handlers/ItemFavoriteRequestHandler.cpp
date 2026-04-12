#include "ItemFavoriteRequestHandler.hpp"
#include "gme/response/ItemFavoriteResponse.hpp"

void Handler::ItemFavoriteRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::ItemFavoriteResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
