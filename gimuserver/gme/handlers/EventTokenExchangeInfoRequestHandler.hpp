#pragma once

#include "../GmeHandler.hpp"

HANDLER_NS_BEGIN
class EventTokenExchangeInfoRequestHandler : public HandlerBase
{
public:
	const char* GetGroupId() const override { return "49zxdfl3"; }
	const char* GetAesKey() const override { return "v2DfDSFl"; }

	void Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const override;
};
HANDLER_NS_END
