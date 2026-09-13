#pragma once

#include <gimuserver/db/Types.h>
#include <drogon/drogon.h>
#include <string>
#include <string_view>

namespace gme
{
struct UserIdentity;

// Full normal PermitPlace slice from current progress, weekday, and key windows.
// Pass the held transaction when called during a mutation (single SQLite connection).
drogon::Task<std::string> buildPermitPlace(db::Database database, UserIdentity identity);

// Replace the generated empty-array placeholder. Throws if its shape is wrong,
// so an accidental empty replacement cannot silently lock the player's map.
void injectPermitPlace(std::string& body, std::string_view permit);
}
