#pragma once

#include "../GmeHandler.hpp"

HANDLER_NS_BEGIN
class ChallengeArenaShopInfoRequestHandler : public HandlerBase
{
public:
	const char* GetGroupId() const override { return "xfmNcJVl"; }
	const char* GetAesKey() const override { return "Fmv6wzeu"; }

	void Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const override;
};
HANDLER_NS_END
