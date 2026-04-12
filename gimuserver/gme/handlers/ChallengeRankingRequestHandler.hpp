#pragma once

#include "../GmeHandler.hpp"

HANDLER_NS_BEGIN
class ChallengeRankingRequestHandler : public HandlerBase
{
public:
	const char* GetGroupId() const override { return "2Kxi7rIB"; }
	const char* GetAesKey() const override { return "v1PzNE9f"; }

	void Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const override;
};
HANDLER_NS_END
