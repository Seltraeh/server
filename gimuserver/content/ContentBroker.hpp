#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*!
* Read-only content broker for the offline-proxy content provider.
*
* WHY THIS EXISTS: the Windows client caches every downloaded asset in its
* LocalState folder and never checks it again, so deploy/game_content stops
* being the source of truth the moment a file is cached (docs/
* OFFLINE_CONTENT_LOADING_PLAN.md).  The proxy's provider replaces the
* client's reads of selected assets with bytes fetched from here, into RAM
* only.  The AppContainer cannot read game_content directly (its folders grant
* nothing to ALL APPLICATION PACKAGES), so loopback is the transport.
*
* What the broker guarantees:
*  - Only files enumerated under <document_root>/content are ever served, by
*    their exact relative path as enumerated.  Reparse points are skipped while
*    enumerating, and every open is re-checked with its final path.
*  - The client namespace is FLAT (LocalState keys files by basename).  A
*    basename is "owned" only when it resolves to exactly one file: a unique
*    name, byte-identical duplicates, or an authored alias.  Differing
*    duplicates without an alias are reported as collisions and never served.
*  - Owned = in the authored scope, an allowed extension, and not excluded.
*    State files (UserDefault.xml, XMLFileProcessor.xml, Ver*.dat, ...) are
*    excluded by the shipped policy so the provider can never shadow them.
*  - Identity is the SHA-256 digest of the copy a name is served from,
*    computed before the name is offered and published in the index.  A
*    change seen by the directory watcher, a rescan after watcher overflow, or
*    a read whose bytes differ from the published identity bumps the
*    generation of exactly that entry, always through the one writer path and
*    compared against the identity still on record, so a same-size edit with
*    a restored timestamp is never swallowed.  The proxy adopts new
*    generations only at safe scene changes, and binds each read to the
*    identity its scene pinned: bytes of any other version are refused (409)
*    rather than mixed into that scene.
*
* Policy: <system_root>/content_provider.json (data only; see
* ContentProviderPolicy).  Absent or "enabled": false keeps the broker dormant.
*/
namespace content
{

/*!
* Authored policy, loaded from <system_root>/content_provider.json.
*/
struct ContentProviderPolicy
{
	//! Runtime switch; the shipped file is false (default-off rollout flag).
	bool enabled = false;
	//! Content root, absolute or relative to the document root; empty = "content".
	//! Lets a test point the broker at a temporary fixture root.
	std::string root;
	//! Relative-path globs ("*" within a segment, "**" across segments) a file must match to be owned.
	std::vector<std::string> scope;
	//! The scope split for matching: entries without a wildcard, lower-cased,
	//! are a set lookup -- a generated family policy lists thousands of exact
	//! paths (scripts/gen_unit_family_policy.py) -- and only the rest are globs.
	std::unordered_set<std::string> scopeExact;
	std::vector<std::string> scopeGlobs;
	//! Basename -> relative path, for basenames whose copies differ.  The target must carry the same basename.
	std::map<std::string, std::string> aliases;
	//! Lower-case extensions (".png") that may be owned; anything else is never served.
	std::vector<std::string> extensions;
	//! Basename globs that are never served, even inside the scope (client state files, MST parts).
	std::vector<std::string> exclude;
	//! Named sets of flat names that are only served together: when one member
	//! cannot be served, every member is reported unavailable, so a scene never
	//! loads half a bundle (a unit's frame layout without its texture).  Bundles
	//! may share members; withdrawing one withdraws every bundle sharing it.
	std::map<std::string, std::vector<std::string>> bundles;
};

/*!
* A table the server's responses carry to the client's startup download lists, as the server's
* cache parsed it at boot (ServerCache::StartupTable; Session 18 correction).  The coverage report
* binds the manifest's inputs.server_files to these -- what this process serves -- never to the
* files on disk now: the cache parses each once and nothing reloads it, so a file edited later
* changes nothing the client is sent until the server restarts (a new epoch).
*/
struct ServedTable
{
	//! "mst": a file the cache parsed; "served" (Session 19): a response the cache synthesizes at boot,
	//! bound by the projection of its served rows (ServerCache::StartupTable).
	std::string root;
	std::string path;       //!< the file under the server's mst_root ("banner_info_mst.json"), or the response key
	std::string digest;     //!< "sha256:<hex>" of the bytes the cache parsed (or of the projection)
	std::string carriedBy;  //!< the response(s) and key carrying its rows ("Initialize Pk5F1vhx")
};

/*!
* The flat names the client's startup paths ask for, derived offline from the
* client's own lists (scripts/startup_coverage.py manifest), loaded from
* <system_root>/content_startup.json.  Data only, strict like the policy.
* Session 16: the coverage report checks every name against the live index,
* so a claim of complete startup coverage always describes one generation.
* QC (2026-10-20): both startup sets are required and non-empty; a name that
* exists nowhere is a gap, never an accepted part of a complete report.
*/
struct StartupManifest
{
	bool loaded = false;
	std::string error;          //!< why it could not be used (absent, malformed, ...)
	std::string digest;         //!< "sha256:<hex>" of the file as read
	std::string build;          //!< the client build its derivation rules were recovered from
	//! Exactly "login" and "background" -> lower-case flat names, sorted, unique, non-empty.
	std::map<std::string, std::vector<std::string>> sets;
	//! Lists the client builds that the generator could not derive: never complete.
	std::vector<std::string> unknown;
	//! Lower-case names left out of every set, each with its documented client-side reason
	//! (the client reports it present itself, client state ...).  Never also in a set.
	std::map<std::string, std::string> excluded;
	//! Lower-case required names the generator found in no source at all (game_content, the
	//! client packages), with where it looked.  Informational: such a name is still required,
	//! and while the broker's tree lacks it the report is incomplete ("absent").
	std::map<std::string, std::string> absent;
	//! The installed client's own files the sets were derived from (inputs.client_files, Session
	//! 18): its master tables in LocalState and its package's resourceMap, each with its SHA-256.
	//! The broker cannot see them: every report lists them ("I" rows) and the DLL compares its
	//! own copies.  A manifest without them never proves complete coverage.
	struct ClientInput
	{
		std::string root;       //!< "localstate" or "package"
		std::string path;       //!< a flat LocalState name, or a '/'-separated path in the package
		std::string digest;     //!< "sha256:<hex>"
	};
	std::vector<ClientInput> clientInputs;
	//! The server's response tables the derived lists were read from (inputs.server_files, Session 18
	//! correction): each a file directly under the server's mst_root ("root": "mst") or a response the
	//! server synthesizes ("served", its key; Session 19), with its SHA-256.  The broker binds them to
	//! the tables it serves (ServedTable); a manifest naming none, another table or other bytes never
	//! proves complete coverage.
	struct ServerInput
	{
		std::string root;       //!< "mst" or "served"
		std::string path;       //!< a file name under mst_root, or a response key
		std::string digest;     //!< "sha256:<hex>"
	};
	std::vector<ServerInput> serverInputs;
};

/*!
* One file the broker offers under a flat basename.
*/
struct OwnedEntry
{
	std::string basename;   //!< As enumerated (original case)
	std::string rel;        //!< Exact relative path, '/'-separated
	uint64_t size = 0;
	uint64_t gen = 0;       //!< Generation of the last change to this owned mapping
	std::string digest;     //!< "sha256:<hex>" of the bytes this generation serves (its identity)
};

/*!
* Result of a file read.
*/
struct ReadResult
{
	int status = 500;               //!< HTTP status to answer with
	std::string error;              //!< Short reason when status != 200
	std::string bytes;
	std::string digest;             //!< "sha256:<hex>" of bytes (409: the version the file has now)
	uint64_t gen = 0;               //!< Generation whose identity is digest
};

class ContentBroker
{
public:
	static ContentBroker& instance();

	/*!
	* Loads the policy and starts the index build + watcher threads.  Never
	* throws: a broken policy or missing root leaves the broker disabled with
	* a reason that /status reports.
	* @param documentRoot drogon document root (deploy/game_content)
	* @param systemRoot server system_root holding content_provider.json
	* @param served the response tables the server's cache parsed for the startup download lists
	*        (ServerCache::startupTables); the manifest's inputs.server_files are bound to them once,
	*        here -- both are fixed for the life of the process
	*/
	void start(const std::filesystem::path& documentRoot, const std::filesystem::path& systemRoot,
		std::vector<ServedTable> served);
	void stop();

	//! Text body for GET /offline_mod/content/v1/status.
	std::string statusText() const;
	/*!
	* Text body for GET /offline_mod/content/v1/index.
	* @param since 0 for a full listing, otherwise only owned entries changed after that generation
	* @param epoch the client's epoch; a mismatch forces a full listing
	*/
	std::string indexText(uint64_t since, const std::string& epoch) const;
	/*!
	* Serves one owned file by its exact relative path.
	* @param expectDigest the identity the client pinned ("sha256:<hex>"), or
	*        empty for the current version.  When the file no longer has that
	*        identity the answer is 409 version-unavailable, never other bytes:
	*        the broker keeps no old versions.
	*/
	ReadResult read(const std::string& rel, const std::string& expectDigest = {});

	/*!
	* Text body for GET /offline_mod/content/v1/coverage (Session 16): whether
	* every name of every startup set is owned and servable at the current
	* generation.  "complete 1" only when the broker is enabled and ready, the
	* manifest is loaded, was derived for @p build and from the response tables
	* this server serves (Session 18 correction), lists no unknown set and the
	* client's own files, and no name is missing, unowned, ambiguous or
	* unavailable.  The result names the epoch and generation it describes; a
	* client must not apply it to another.
	* @param build the client build identity the proxy verified ("6253F827-01756000-00ED273D")
	* @param detail list every problem name, not only the first few per set
	*/
	std::string coverageText(const std::string& build, bool detail) const;

	bool enabled() const { return m_enabled.load(); }
	bool ready() const { return m_ready.load(); }
	const std::string& epoch() const { return m_epoch; }

private:
	struct FileEntry
	{
		std::string rel;
		uint64_t size = 0;
		int64_t mtime = 0;
		std::string digest;      //!< Empty until hashed (a served copy and compared duplicates always are)
		uint64_t gen = 1;        //!< Generation of the last content change
	};

	ContentBroker() = default;

	//! Outcome of recording bytes a read saw (publishObserved).
	enum class Observed { Published, ChangedAgain, NotOwned };

	void buildThread();
	void watchThread();
	//! Re-walks the tree and applies what changed; returns what applyChanged could not examine.
	std::vector<std::string> rescan(const char* why);
	/*!
	* Re-examines @p rels and publishes what changed as one generation.  Returns
	* the names it could not examine this instant (held delete-pending or
	* exclusively in the middle of a replace): their records are kept as they
	* were, and the watcher looks at them again.
	*/
	std::vector<std::string> applyChanged(const std::vector<std::string>& rels, const char* why);
	/*!
	* A read saw bytes whose digest differs from the published identity.
	* Records the change through the writer path (re-hashing the file against
	* the identity still on record) and reports the generation whose identity
	* is @p observed, or that the file moved on again meanwhile.
	*/
	Observed publishObserved(const std::string& rel, const std::string& observed, uint64_t& gen);
	void recomputeOwned(uint64_t newGen);
	bool inScope(const std::string& rel) const;
	bool allowedName(const std::string& basename) const;
	std::string hashFile(const std::filesystem::path& p, uint64_t& size, int64_t& mtime, std::string& error) const;

	std::filesystem::path m_root;          //!< canonical <document_root>/content
	std::wstring m_rootFinal;              //!< canonical root as a final-path prefix (Windows)
	ContentProviderPolicy m_policy;
	std::string m_policyDigest;            //!< "sha256:<hex>" of content_provider.json as loaded
	StartupManifest m_manifest;
	std::vector<ServedTable> m_served;     //!< the response tables this process serves (start)
	//! Why the manifest is not bound to m_served; empty when it is (or when there is no manifest).
	//! Computed once in start(): neither side changes while the server runs.
	std::string m_serverBinding;

	//! Per-name classes of the last coverage computation, keyed by the generation it saw.
	struct CoverageCache
	{
		uint64_t generation = 0;
		bool valid = false;
		//! set -> class -> lower-case names ("absent", "unowned", "missing", "ambiguous", "unavailable").
		std::map<std::string, std::map<std::string, std::vector<std::string>>> problems;
		std::map<std::string, size_t> ok;  //!< set -> owned and servable names
	};
	mutable std::mutex m_coverageLock;
	mutable CoverageCache m_coverage;
	std::string m_reason;                  //!< why disabled / not ready
	std::string m_epoch;

	std::atomic<bool> m_enabled{ false };
	std::atomic<bool> m_ready{ false };
	std::atomic<bool> m_stop{ false };
	std::atomic<uint64_t> m_generation{ 0 };

	mutable std::shared_mutex m_lock;
	std::recursive_mutex m_writer;         //!< serialises index updates (build, watcher, digest-on-read)
	std::unordered_map<std::string, FileEntry> m_files;          //!< rel -> entry
	std::unordered_map<std::string, OwnedEntry> m_owned;         //!< lower basename -> owned entry
	std::unordered_map<std::string, uint64_t> m_tombstones;      //!< lower basename -> gen it stopped being owned
	std::map<std::string, std::vector<std::string>> m_collisions; //!< lower basename -> differing in-scope copies
	std::map<std::string, std::pair<uint64_t, std::string>> m_unavailable; //!< lower basename -> (gen, reason): incomplete bundles
	uint64_t m_skippedReparse = 0;
	uint64_t m_rescans = 0;
	uint64_t m_overflows = 0;

	std::thread m_builder;
	std::thread m_watcher;
	void* m_stopEvent = nullptr;           //!< HANDLE on Windows
};

} // namespace content
