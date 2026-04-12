#include "CampaignReceiptRequestHandler.hpp"
#include "gme/response/CampaignReceiptResponse.hpp"

void Handler::CampaignReceiptRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;
	Response::CampaignReceiptResponse resp;
	resp.Serialize(res);
	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
