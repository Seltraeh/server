#include "App.hpp"
#include "OfflineModController.hpp"

#include <gimuserver/content/ContentBroker.hpp>

#include <charconv>

using namespace drogon;

void OfflineModController::HandleFpsCap(const HttpRequestPtr& rq,
	std::function<void(const HttpResponsePtr&)>&& callback)
{
	(void)rq;
	const auto cap = theServer()->cache().serverConfig().fpsCap;

	auto resp = HttpResponse::newHttpResponse();
	resp->setContentTypeCode(ContentType::CT_TEXT_PLAIN);
	resp->setStatusCode(k200OK);
	resp->setBody(std::to_string(cap));
	resp->setCloseConnection(true);

	callback(resp);
}

namespace
{

HttpResponsePtr textResponse(HttpStatusCode code, std::string body)
{
	auto resp = HttpResponse::newHttpResponse();
	resp->setContentTypeCode(ContentType::CT_TEXT_PLAIN);
	resp->setStatusCode(code);
	resp->addHeader("Cache-Control", "no-store");
	resp->setBody(std::move(body));
	return resp;
}

} // namespace

void OfflineModController::HandleContentStatus(const HttpRequestPtr& rq,
	std::function<void(const HttpResponsePtr&)>&& callback)
{
	(void)rq;
	callback(textResponse(k200OK, content::ContentBroker::instance().statusText()));
}

void OfflineModController::HandleContentIndex(const HttpRequestPtr& rq,
	std::function<void(const HttpResponsePtr&)>&& callback)
{
	auto& broker = content::ContentBroker::instance();
	if (!broker.enabled() || !broker.ready())
	{
		callback(textResponse(k503ServiceUnavailable, "bfcp 2\nerror not-ready\nend\n"));
		return;
	}
	uint64_t since = 0;
	const auto sinceText = rq->getParameter("since");
	if (!sinceText.empty())
	{
		const auto [ptr, ec] = std::from_chars(sinceText.data(), sinceText.data() + sinceText.size(), since);
		if (ec != std::errc{} || ptr != sinceText.data() + sinceText.size())
		{
			callback(textResponse(k400BadRequest, "bfcp 2\nerror bad-since\nend\n"));
			return;
		}
	}
	callback(textResponse(k200OK, broker.indexText(since, rq->getParameter("epoch"))));
}

void OfflineModController::HandleContentCoverage(const HttpRequestPtr& rq,
	std::function<void(const HttpResponsePtr&)>&& callback)
{
	const auto text = content::ContentBroker::instance().coverageText(rq->getParameter("build"),
		rq->getParameter("detail") == "1");
	callback(textResponse(k200OK, text));
}

void OfflineModController::HandleContentFile(const HttpRequestPtr& rq,
	std::function<void(const HttpResponsePtr&)>&& callback)
{
	auto& broker = content::ContentBroker::instance();
	const auto rel = rq->getParameter("p");
	// d = the identity the client's scene pinned; another version is refused (409).
	auto r = broker.read(rel, rq->getParameter("d"));
	if (r.status != 200)
	{
		LOG_WARN << "content broker: " << rq->getQuery() << " -> " << r.status << " " << r.error;
		auto resp = textResponse(static_cast<HttpStatusCode>(r.status), "bfcp 2\nerror " + r.error + "\nend\n");
		resp->addHeader("X-BFCP-Error", r.error);
		if (r.status == 409)
		{
			// The version the file has now, for the client's log only.
			resp->addHeader("X-BFCP-Digest", r.digest);
			resp->addHeader("X-BFCP-Gen", std::to_string(r.gen));
			resp->addHeader("X-BFCP-Epoch", broker.epoch());
		}
		callback(resp);
		return;
	}
	LOG_INFO << "content broker: served " << rel << " (" << r.bytes.size() << " bytes, gen " << r.gen
	         << ", " << r.digest.substr(0, 7 + 16) << ")";
	auto resp = HttpResponse::newHttpResponse();
	resp->setContentTypeCode(ContentType::CT_APPLICATION_OCTET_STREAM);
	resp->setStatusCode(k200OK);
	resp->addHeader("Cache-Control", "no-store");
	resp->addHeader("X-BFCP-Digest", r.digest);
	resp->addHeader("X-BFCP-Gen", std::to_string(r.gen));
	resp->addHeader("X-BFCP-Epoch", broker.epoch());
	resp->setBody(std::move(r.bytes));
	callback(resp);
}
