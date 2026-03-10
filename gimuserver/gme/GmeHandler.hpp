#pragma once

// GmeHandler — base class and helpers for all GME request handlers.
//
//  HandlerBase        - Abstract base that every game-action handler inherits from.
//                       Subclasses must provide:
//                         GetGroupId()  - the obfuscated request-id key used to
//                                         route the request from GmeController.
//                         GetAesKey()   - the per-handler AES key for decrypting
//                                         the request body and encrypting the response.
//                         Handle()      - the actual business logic.
//  DrogonCallback     - Alias for the Drogon HTTP response callback type.
//  newGmeOkResponse   - Builds a 200 response with an AES-encrypted JSON body.
//  newGmeErrorResponse - Builds an error response with the GME error envelope.

#include "GmeTypes.hpp"
#include "GmeRequest.hpp"
#include "GmeTypes.hpp"
#include "UserInfo.hpp"

#include <json/value.h>

#include <drogon/HttpResponse.h>
#include <drogon/Session.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Exception.h>

#define HANDLER_NS_BEGIN namespace Handler {
#define HANDLER_NS_END }

HANDLER_NS_BEGIN
class HandlerBase;
using DrogonCallback = std::function<void(const drogon::HttpResponsePtr&)>;

class HandlerBase
{
public:
	virtual const char* GetGroupId() const = 0;
	virtual const char* GetAesKey() const = 0;
	virtual void Handle(UserInfo& session, DrogonCallback cb, const Json::Value& req) const = 0;

protected:
	void OnError(const drogon::orm::DrogonDbException& e, DrogonCallback cb, ErrorOperation op = ErrorOperation::Close) const;
};
HANDLER_NS_END

drogon::HttpResponsePtr newGmeErrorResponse(const std::string& reqId, ErrorID errId, ErrorOperation errContinueOp, const std::string& msg);
drogon::HttpResponsePtr newGmeOkResponse(const std::string& reqId, const std::string& aesKey, const Json::Value& data);
