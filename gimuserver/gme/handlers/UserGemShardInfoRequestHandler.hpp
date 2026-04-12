#pragma once

#include "../GmeHandler.hpp"

HANDLER_NS_BEGIN
class UserGemShardInfoRequestHandler : public HandlerBase
{
public:
	const char* GetGroupId() const override { return "29slks49"; }
	const char* GetAesKey() const override { return "930sDd3i"; }

	void Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const override;
};
HANDLER_NS_END
