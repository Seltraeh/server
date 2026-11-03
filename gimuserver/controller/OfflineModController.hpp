#pragma once

/*!
* Endpoints consumed by the offline-proxy libcurl shim (not by the BF
* client itself). These deliver server-owned configuration that the
* client-side hooks need at startup.
*
* <p>URL prefix: /offline_mod/
*/
class OfflineModController : public drogon::HttpController<OfflineModController>
{
public:
	/*!
	* Returns the desired client render-loop cap as a bare integer in
	* the response body (e.g. "60"). Read by the offline-proxy FPS hook
	* on its first MyRender call; falls back to the proxy's compile-time
	* default if this endpoint is unreachable.
	* @param[in] rq HTTP request
	* @param[in] callback Callback to send the response
	*/
	void HandleFpsCap(const drogon::HttpRequestPtr& rq, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

	/*!
	* Content provider broker status (protocol "bfcp 2", plain text lines).
	* The proxy activates its content provider only when this reports
	* "enabled 1" and "ready 1".  See content::ContentBroker.
	*/
	void HandleContentStatus(const drogon::HttpRequestPtr& rq, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

	/*!
	* Owned-file index: "F gen size basename relpath digest" rows (digest =
	* the identity reads are bound to), "D gen basename" removals (deltas
	* only), "X gen basename reason" unavailable names and "C basename
	* relpath..." collisions, ending in "end".  Query: since=<generation>
	* (0 = full), epoch=<server epoch>.
	*/
	void HandleContentIndex(const drogon::HttpRequestPtr& rq, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

	/*!
	* Bytes of one owned file, by its exact relative path (query p=).  Headers
	* X-BFCP-Digest (sha256:<hex>), X-BFCP-Gen and X-BFCP-Epoch identify the
	* content; nothing else under the document root is reachable here.  With
	* d=<sha256:hex> the read is bound to that version: 409
	* version-unavailable when the file no longer has it.
	*/
	void HandleContentFile(const drogon::HttpRequestPtr& rq, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

	/*!
	* Startup coverage (Session 16): whether every name the client's startup
	* lists ask for (content_startup.json) is owned and servable now, for the
	* generation it names.  Query: build=<client build identity> (required for
	* "complete 1"), detail=1 lists every problem name.  Always 200: an
	* incomplete or unusable report is an answer, not an error.
	*/
	void HandleContentCoverage(const drogon::HttpRequestPtr& rq, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

	METHOD_LIST_BEGIN
		ADD_METHOD_TO(OfflineModController::HandleFpsCap, "/offline_mod/fps_cap", drogon::Get);
		ADD_METHOD_TO(OfflineModController::HandleContentStatus, "/offline_mod/content/v1/status", drogon::Get);
		ADD_METHOD_TO(OfflineModController::HandleContentIndex, "/offline_mod/content/v1/index", drogon::Get);
		ADD_METHOD_TO(OfflineModController::HandleContentFile, "/offline_mod/content/v1/file", drogon::Get);
		ADD_METHOD_TO(OfflineModController::HandleContentCoverage, "/offline_mod/content/v1/coverage", drogon::Get);
	METHOD_LIST_END
};
