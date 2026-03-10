#pragma once

// BfCrypt — AES-based encryption/decryption for the Brave Frontier GME protocol.
//
//  CryptSREE  - Encodes a JSON value into the SREE (account API) wire format.
//  CryptGME   - Encrypts a JSON response body with the handler-specific AES key
//               before sending it back to the game client.
//  DecryptGME - Decrypts an inbound GME request body that was encrypted by the
//               client using the same handler-specific AES key.

#include <json/value.h>

namespace BfCrypt
{
	std::string CryptSREE(const Json::Value& v);

	std::string CryptGME(const Json::Value& v, const std::string& key);
	void DecryptGME(const std::string& in, const std::string& key, Json::Value& root);
}
