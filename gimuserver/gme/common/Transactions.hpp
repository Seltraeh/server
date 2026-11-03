#pragma once

#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <drogon/utils/coroutine.h>
#include <trantor/utils/Logger.h>

#include <atomic>
#include <memory>
#include <stdexcept>

namespace gme
{

using TransactionPtr = std::shared_ptr<drogon::orm::Transaction>;

namespace detail
{
inline std::atomic<bool>& commitFaultArmed()
{
	static std::atomic<bool> armed{ false };
	return armed;
}

/*!
* The handle callers hold: when its last copy goes, whatever the request did
* not commit is ROLLED BACK.
*
* ⚠ WHY: Drogon's own destructor COMMITS a transaction nobody committed or
* rolled back.  A handler that threw a non-database exception after some
* writes (FixGiftInfo catches only database exceptions, and its transaction
* lives inside that try), or refused after writing without a rollback, kept
* those writes while the client was told the request failed -- partial
* gameplay (scripts/test_db_exception_atomicity_wire.py).  Most handlers roll
* back in a catch-all, but that was each handler's own discipline; this makes
* it the transaction's.  After commitTransaction the transaction holds only
* the empty BEGIN, so this rollback changes nothing there.
*
* A ROLLBACK statement, then an empty BEGIN for Drogon's destructor to commit
* -- not Drogon's rollback().  When the handler already rolled back (most
* refusals do), Drogon's rollback() is rejected and logs "Transaction roll back
* error" for nothing; a statement on a rolled-back transaction instead fails
* with TransactionRollback, which is ignored here.  The statements' callbacks
* keep the transaction alive until they have run.
*/
inline TransactionPtr rollbackOnRelease(TransactionPtr transaction)
{
	auto* const raw = transaction.get();
	return TransactionPtr(raw, [held = std::move(transaction)](drogon::orm::Transaction*) mutable noexcept {
		try
		{
			auto keep = held;
			keep->execSqlAsync("ROLLBACK",
				[keep](const drogon::orm::Result&) {
					keep->execSqlAsync("BEGIN",
						[](const drogon::orm::Result&) {},
						[](const drogon::orm::DrogonDbException&) {});
				},
				[](const drogon::orm::DrogonDbException&) {});
		}
		catch (...)
		{
		}
		held.reset();
	});
}
} // namespace detail

/*!
* Test fault (debug console `dbfault`): the NEXT commitTransaction throws a
* non-database exception instead of committing -- a handler failing after all
* of its writes.  One shot.  scripts/test_db_exception_atomicity_wire.py.
*/
inline void armCommitFault()
{
	detail::commitFaultArmed() = true;
}

/*!
* Commits a transaction NOW, through the transaction itself, and returns only
* once the commit is durable.  A commit that fails throws.
*
* ⚠ WHY: Drogon (1.9.10) commits a transaction from its DESTRUCTOR.  The
* "commit" is queued on the connection after the handler has returned -- after
* the client was already answered -- and when it fails (SQLITE_BUSY once an
* outside reader of the save outlasts busy_timeout) Drogon only logs
* "Transaction submission failed": no retry, no rollback.  The acknowledged
* action was never written, and the one connection went back to the pool still
* inside the open transaction, so every later request failed at "transaction
* begin" until a restart (scripts/test_db_commit_contention_wire.py).
*
* A COMMIT issued as a statement OF the transaction runs while the transaction
* still owns its connection.  When it fails, Drogon's statement-failure path
* rolls the transaction back on that same connection before releasing it -- the
* connection stays usable -- and the exception reaches the handler, which then
* answers with an error.  Nothing is acknowledged that was not written.  The
* BEGIN after a successful COMMIT leaves an empty transaction, which the
* handle's release rolls back (detail::rollbackOnRelease) -- no lock, no change.
*
* It also throws for a transaction Drogon already rolled back (a statement in
* it failed and the handler caught that): a success reply for that work would
* acknowledge writes that were undone.
*
* Make it the transaction's last statement, immediately before the reply:
* statements after it run in that new transaction and are rolled back when the
* handle is released.  scripts/audit_transactions.py checks that every success
* reply after a transaction starts is preceded by one.
*
* This replaces gme::CommitTransaction, which awaited Drogon's destructor
* commit: it stopped false replies but left the connection open on a failure.
*
* @param transaction The transaction to commit.
*/
inline drogon::Task<> commitTransaction(const TransactionPtr& transaction)
{
	if (detail::commitFaultArmed().exchange(false))
		throw std::runtime_error("injected fault before COMMIT (debug console dbfault)");
	co_await transaction->execSqlCoro("COMMIT");
	try
	{
		co_await transaction->execSqlCoro("BEGIN");
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		// The COMMIT above is already durable; Drogon then marks the
		// transaction rolled back and its destructor commits nothing.
		LOG_WARN << "commitTransaction: BEGIN after a durable COMMIT failed: " << ex.base().what();
	}
}

/*!
* commitTransaction for a handler that answers a failed commit with its own
* refusal rather than an error.  The transaction is rolled back either way.
*
* @param transaction The transaction to commit.
* @return true once the commit is durable; false if it failed (logged).
*/
inline drogon::Task<bool> tryCommitTransaction(const TransactionPtr& transaction)
{
	try
	{
		co_await commitTransaction(transaction);
		co_return true;
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		LOG_ERROR << "tryCommitTransaction: commit failed, rolled back: " << ex.base().what();
	}
	co_return false;
}

/*!
* Starts a transaction, first clearing a connection a failed commit left open.
* The handle it returns rolls back, when released, whatever was not committed
* (detail::rollbackOnRelease): Drogon's destructor never commits a gme
* transaction's work, only the empty BEGIN the release leaves.
*
* A connection can still be found inside a transaction if something other
* than a gme transaction left it so (the debug console's dbwedge does, for
* the test).  Drogon's next "begin" then fails ("cannot start a transaction
* within a transaction") and the new transaction is born rolled back.  The
* probe below sees exactly that, rolls the abandoned transaction back (it was
* never committed, so nothing of it was acknowledged by commitTransaction) and
* starts again.  Normally the probe costs one SELECT; Drogon runs it after
* "begin".
*
* @param database Database client to start the transaction on.
* @return A transaction whose BEGIN succeeded.
*/
inline drogon::Task<TransactionPtr> beginTransaction(const drogon::orm::DbClientPtr& database)
{
	for (int attempt = 0;; ++attempt)
	{
		auto transaction = co_await database->newTransactionCoro();
		try
		{
			co_await transaction->execSqlCoro("SELECT 1");
			co_return detail::rollbackOnRelease(std::move(transaction));
		}
		catch (const drogon::orm::TransactionRollback&)
		{
			if (attempt > 0)
				throw;
		}
		LOG_ERROR << "beginTransaction: the connection was still inside a transaction"
			" (a commit that failed after its reply); rolling that transaction back";
		try
		{
			co_await database->execSqlCoro("ROLLBACK");
		}
		catch (const drogon::orm::DrogonDbException& ex)
		{
			LOG_ERROR << "beginTransaction: ROLLBACK of the abandoned transaction failed: "
				<< ex.base().what();
		}
	}
}

} // namespace gme
