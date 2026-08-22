#pragma once

#include <gimuserver/db/Types.h>
#include <gimuserver/packets/all.hpp>

#include <drogon/drogon.h>

#include <string>

namespace gme
{
struct UserIdentity;

/*!
* Loads the authored Journal from archive_root/summoner_journal.json.
*
* @param archiveRoot Configured archive_root.
*/
void loadSummonerJournalArchive(const std::string& archiveRoot);

/*!
* Builds the whole Summoner's Journal reply.
*
* The captured 32Gwida0 request carries no parameters at all — identity,
* signal key and the MST manifest — so the entire screen has to be assembled
* here from the archive plus this user's state.
*
* Progress is reported as 0 for every mission, because nothing feeds it yet
* (every archive row is `tracked: false`).  The screen renders correctly in
* that state; it simply cannot advance.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @return The populated response.
*/
drogon::Task<::SummonerJournalInfoResp> buildSummonerJournal(
	db::Database database,
	const UserIdentity& identity);

} // namespace gme
