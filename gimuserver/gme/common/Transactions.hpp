#pragma once

#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>

#include <coroutine>
#include <memory>
#include <utility>

namespace gme
{

/*!
* Commits a transaction and waits for the result.
*
* Drogon commits a transaction when its last reference goes away and reports
* the outcome only through the commit callback, so a handler that returns
* success as soon as it has prepared its reply can tell the client about a
* charge the database never kept (FeSkillGet's first test run caught a reader
* arriving between the reply and the asynchronous commit).  Awaiting this
* hands over the handler's reference and resumes with the commit's result:
*
*     if (!(co_await gme::CommitTransaction(std::move(transaction))))
*         co_return HandleResult::refuseToHome(...);
*
* The caller must hold the only reference -- anything else keeping the
* transaction alive keeps it open, and the await never resumes.
*/
struct CommitTransaction : drogon::CallbackAwaiter<bool>
{
	explicit CommitTransaction(std::shared_ptr<drogon::orm::Transaction> transaction)
		: tx(std::move(transaction))
	{
	}

	std::shared_ptr<drogon::orm::Transaction> tx;

	void await_suspend(std::coroutine_handle<> handle)
	{
		auto transaction = std::move(tx);
		transaction->setCommitCallback([this, handle](bool ok) {
			setValue(ok);
			handle.resume();
		});
		transaction.reset();
	}
};

} // namespace gme
