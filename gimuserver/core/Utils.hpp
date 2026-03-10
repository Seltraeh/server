#pragma once

// Utils — miscellaneous helper functions used across the server.
//
//  DumpInfoToDrogon          - Logs HTTP request details through Drogon's logging system.
//  GetDrogonBindHostname     - Returns the HTTPS bind address configured in config.json.
//  GetDrogonHttpBindHostname - Returns the plain HTTP bind address.
//  RandomUserID              - Generates a random user ID string for new accounts.
//  RandomAccountID           - Generates a random account ID string for new accounts.
//  AppendJsonReqToFile       - Writes a decoded request body to the request log directory.
//  AppendJsonResToFile       - Writes a response body to the response log directory.
//  AddMissingDlcFile         - Records a 404'd DLC asset path to dlc_error_file.

#include <drogon/HttpRequest.h>
#include <drogon/Session.h>
#include <gme/GmeTypes.hpp>

namespace Utils
{
	void DumpInfoToDrogon(const drogon::HttpRequestPtr& rq, const std::string& ip);
	std::string GetDrogonBindHostname();
	std::string GetDrogonHttpBindHostname();
	std::string RandomUserID();
	std::string RandomAccountID();
	void AppendJsonReqToFile(const Json::Value& v, const std::string& group);
	void AppendJsonResToFile(const Json::Value& v, const std::string& group);
	void AddMissingDlcFile(const std::string& v);
}
