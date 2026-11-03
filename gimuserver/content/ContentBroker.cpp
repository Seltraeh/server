#include "App.hpp"
#include "ContentBroker.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <set>
#include <sstream>

#include <cryptopp/sha.h>
#include <cryptopp/hex.h>
#include <cryptopp/filters.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace content
{

namespace
{

// The provider protocol is plain text so the 32-bit proxy needs no JSON
// library; a tab or newline inside a name would break a line, so such names
// are never offered.  Version 2 publishes each name's identity (digest) in
// the index and binds reads to it; a proxy built for version 1 declines it.
constexpr const char* kProtocol = "bfcp 2";

// Small on purpose: an overflow only costs a rescan, and keeping it small makes
// the overflow path reachable by an ordinary burst of edits (and by the tests).
constexpr size_t kWatchBufferBytes = 16 * 1024;
constexpr auto kDebounce = std::chrono::milliseconds(200);
constexpr int kSecondLooks = 25;   //!< re-examinations of a name caught mid-replace (~5 s of debounce periods)

std::string lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::string basenameOf(const std::string& rel)
{
	const auto slash = rel.find_last_of('/');
	return slash == std::string::npos ? rel : rel.substr(slash + 1);
}

std::string extensionOf(const std::string& name)
{
	const auto dot = name.find_last_of('.');
	return dot == std::string::npos ? std::string{} : lower(name.substr(dot));
}

bool safeProtocolText(const std::string& s)
{
	if (s.empty())
		return false;
	for (const unsigned char c : s)
		if (c < 0x20 || c == 0x7f)
			return false;
	return true;
}

//! Free text (a parser's message) made safe for one protocol line.
std::string oneLine(std::string s)
{
	for (auto& c : s)
		if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f)
			c = ' ';
	return s;
}

//! "sha256:" followed by 64 lower-case hex digits, as the broker writes identities.
bool isDigestTag(const std::string& s)
{
	constexpr std::string_view prefix = "sha256:";
	if (s.size() != prefix.size() + 64 || !s.starts_with(prefix))
		return false;
	return std::all_of(s.begin() + prefix.size(), s.end(),
		[](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

std::string sha256Tag(const std::string& bytes)
{
	CryptoPP::SHA256 sha;
	CryptoPP::byte raw[CryptoPP::SHA256::DIGESTSIZE];
	sha.CalculateDigest(raw, reinterpret_cast<const CryptoPP::byte*>(bytes.data()), bytes.size());
	std::string hex;
	CryptoPP::HexEncoder enc(new CryptoPP::StringSink(hex), false);
	enc.Put(raw, sizeof raw);
	enc.MessageEnd();
	return "sha256:" + hex;
}

/*!
* Case-insensitive glob: '*' and '?' stay within one path segment, '**'
* crosses segments.  Patterns are authored in content_provider.json.
*/
bool globMatch(const char* p, const char* s)
{
	while (*p)
	{
		if (p[0] == '*' && p[1] == '*')
		{
			p += 2;
			if (*p == '/')
				++p;
			if (!*p)
				return true;
			for (const char* t = s; ; ++t)
			{
				if (globMatch(p, t))
					return true;
				if (!*t)
					return false;
			}
		}
		if (*p == '*')
		{
			++p;
			for (const char* t = s; ; ++t)
			{
				if (globMatch(p, t))
					return true;
				if (!*t || *t == '/')
					return false;
			}
		}
		if (!*s)
			return false;
		if (*p == '?')
		{
			if (*s == '/')
				return false;
		}
		else if (std::tolower(static_cast<unsigned char>(*p)) != std::tolower(static_cast<unsigned char>(*s)))
			return false;
		++p;
		++s;
	}
	return !*s;
}

std::string toUtf8(const fs::path& p)
{
	const auto u8 = p.generic_u8string();
	return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

fs::path fromUtf8(const std::string& s)
{
	return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

std::string randomEpoch()
{
	std::random_device rd;
	std::mt19937_64 gen((static_cast<uint64_t>(rd()) << 32) ^ rd()
		^ static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::ostringstream o;
	o << std::hex << gen();
	return o.str();
}

/*!
* Reads content_provider.json.  The file is data only: an unknown key is an
* error rather than silently ignored, so a typo cannot widen or disable the
* scope without being reported.
*/
bool loadPolicy(const fs::path& file, ContentProviderPolicy& out, std::string& error)
{
	std::ifstream in(file, std::ios::binary);
	if (!in)
	{
		error = "policy file absent: " + file.string();
		return false;
	}
	std::stringstream ss;
	ss << in.rdbuf();

	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream src(ss.str());
	if (!Json::parseFromStream(builder, src, &root, &errs) || !root.isObject())
	{
		error = "policy file is not a JSON object: " + errs;
		return false;
	}
	static const std::set<std::string> kKeys{ "enabled", "root", "scope", "aliases", "extensions", "exclude", "bundles" };
	for (const auto& key : root.getMemberNames())
	{
		if (!kKeys.contains(key))
		{
			error = "policy file has an unknown key: " + key;
			return false;
		}
	}

	const auto strings = [&](const char* key, std::vector<std::string>& dst) {
		const auto& v = root[key];
		if (v.isNull())
			return true;
		if (!v.isArray())
		{
			error = std::string("policy key is not an array: ") + key;
			return false;
		}
		for (const auto& item : v)
		{
			if (!item.isString() || item.asString().empty())
			{
				error = std::string("policy array holds a non-string: ") + key;
				return false;
			}
			dst.push_back(item.asString());
		}
		return true;
	};

	out = {};
	out.enabled = root.get("enabled", false).asBool();
	if (!root["root"].isNull())
	{
		if (!root["root"].isString() || root["root"].asString().empty())
		{
			error = "policy root is not a non-empty string";
			return false;
		}
		out.root = root["root"].asString();
	}
	if (!strings("scope", out.scope) || !strings("extensions", out.extensions) || !strings("exclude", out.exclude))
		return false;
	for (auto& ext : out.extensions)
		ext = lower(ext);
	for (const auto& pattern : out.scope)
	{
		if (pattern.find_first_of("*?") == std::string::npos)
			out.scopeExact.insert(lower(pattern));
		else
			out.scopeGlobs.push_back(pattern);
	}

	const auto& bundles = root["bundles"];
	if (!bundles.isNull())
	{
		if (!bundles.isObject())
		{
			error = "policy bundles is not an object";
			return false;
		}
		for (const auto& name : bundles.getMemberNames())
		{
			const auto& members = bundles[name];
			if (!members.isArray() || members.empty())
			{
				error = "policy bundle is not a non-empty array: " + name;
				return false;
			}
			auto& out_members = out.bundles[name];
			for (const auto& m : members)
			{
				const auto member = m.isString() ? m.asString() : std::string{};
				if (member.empty() || member.find('/') != std::string::npos || member.find('\\') != std::string::npos
					|| !safeProtocolText(member))
				{
					error = "policy bundle member must be a flat file name: " + name;
					return false;
				}
				out_members.push_back(lower(member));
			}
		}
	}

	const auto& aliases = root["aliases"];
	if (!aliases.isNull())
	{
		if (!aliases.isObject())
		{
			error = "policy aliases is not an object";
			return false;
		}
		for (const auto& name : aliases.getMemberNames())
		{
			const auto& target = aliases[name];
			if (!target.isString())
			{
				error = "policy alias target is not a string: " + name;
				return false;
			}
			const auto rel = target.asString();
			// An alias picks WHICH copy of a basename the flat client namespace
			// sees; it may not rename a file, or the client would read bytes
			// under a name the package never had.
			if (lower(basenameOf(rel)) != lower(name) || rel.find('\\') != std::string::npos
				|| rel.find("..") != std::string::npos || rel.starts_with('/'))
			{
				error = "policy alias must select a copy with the same basename: " + name + " -> " + rel;
				return false;
			}
			out.aliases[lower(name)] = rel;
		}
	}
	return true;
}

/*!
* How a server input is named in a reason and in the binding: a file by its name, a response the
* server synthesizes as "served:<key>" (scripts/startup_coverage.py server_input_name).
*/
std::string serverInputName(const std::string& root, const std::string& path)
{
	return root == "mst" ? path : root + ":" + path;
}

/*!
* Reads content_startup.json (Session 16), written by
* scripts/startup_coverage.py manifest.  Strict like the policy: an unknown key,
* a non-flat name, another format, a missing, empty or unknown set, a name twice,
* a reasonless or contradictory exclusion is an error, and an unusable manifest
* makes every coverage report incomplete rather than silently smaller.
*/
bool loadManifest(const fs::path& file, StartupManifest& out, std::string& error)
{
	out = {};
	std::ifstream in(file, std::ios::binary);
	if (!in)
	{
		error = "startup manifest absent: " + file.string();
		return false;
	}
	std::stringstream ss;
	ss << in.rdbuf();
	const std::string bytes = ss.str();
	out.digest = sha256Tag(bytes);

	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream src(bytes);
	if (!Json::parseFromStream(builder, src, &root, &errs) || !root.isObject())
	{
		error = "startup manifest is not a JSON object: " + errs;
		return false;
	}
	static const std::set<std::string> kKeys{ "format", "build", "sets", "unknown", "excluded", "absent", "inputs",
		"generated" };
	for (const auto& key : root.getMemberNames())
	{
		if (!kKeys.contains(key))
		{
			error = "startup manifest has an unknown key: " + key;
			return false;
		}
	}
	if (root.get("format", "").asString() != "bfcp-startup 1")
	{
		error = "startup manifest format is not \"bfcp-startup 1\"";
		return false;
	}
	if (!root["build"].isString() || root["build"].asString().empty() || !safeProtocolText(root["build"].asString()))
	{
		error = "startup manifest names no client build";
		return false;
	}
	out.build = root["build"].asString();
	const auto flatName = [](const Json::Value& v, std::string& name) {
		if (!v.isString())
			return false;
		name = v.asString();
		return !name.empty() && name.find('/') == std::string::npos && name.find('\\') == std::string::npos
			&& safeProtocolText(name) && name.find('\t') == std::string::npos;
	};
	// The two lists the startup paths build -- the login priority window and the background
	// check list -- are both required, both non-empty, and nothing else is a set.  A manifest
	// that omits one or empties it would make the report complete by asking for less (QC
	// 2026-10-20: a login-only manifest and one with two empty sets both returned complete 1).
	static const std::set<std::string> kSets{ "login", "background" };
	const auto& sets = root["sets"];
	if (!sets.isObject())
	{
		error = "startup manifest sets is not an object";
		return false;
	}
	for (const auto& set : sets.getMemberNames())
	{
		if (!kSets.contains(set))
		{
			error = "startup manifest has an unknown set"
				+ (safeProtocolText(set) && set.find('\t') == std::string::npos ? ": " + set : std::string{});
			return false;
		}
	}
	for (const auto& set : kSets)
	{
		const auto& names = sets[set];
		if (!sets.isMember(set) || !names.isArray() || names.empty())
		{
			error = "startup manifest lacks the required non-empty set " + set;
			return false;
		}
		auto& dst = out.sets[set];
		for (const auto& v : names)
		{
			std::string name;
			if (!flatName(v, name))
			{
				error = "startup manifest set " + set + " holds a non-flat name";
				return false;
			}
			dst.push_back(lower(name));
		}
		std::sort(dst.begin(), dst.end());
		const auto twice = std::adjacent_find(dst.begin(), dst.end());
		if (twice != dst.end())
		{
			error = "startup manifest set " + set + " names " + *twice + " twice";
			return false;
		}
	}
	const auto& unknown = root["unknown"];
	if (!unknown.isNull())
	{
		if (!unknown.isArray())
		{
			error = "startup manifest unknown is not an array";
			return false;
		}
		for (const auto& v : unknown)
		{
			if (!v.isString() || v.asString().empty() || !safeProtocolText(v.asString()))
			{
				error = "startup manifest unknown holds a non-string";
				return false;
			}
			out.unknown.push_back(v.asString());
		}
	}
	for (const char* key : { "excluded", "absent" })
	{
		const auto& map = root[key];
		if (map.isNull())
			continue;
		if (!map.isObject())
		{
			error = std::string("startup manifest ") + key + " is not an object";
			return false;
		}
		auto& dst = std::string(key) == "excluded" ? out.excluded : out.absent;
		for (const auto& name : map.getMemberNames())
		{
			std::string flat;
			// Every entry carries its reason: an exclusion without its documented client-side
			// evidence is not one.
			if (!flatName(Json::Value(name), flat) || !map[name].isString() || map[name].asString().empty())
			{
				error = std::string("startup manifest ") + key + " entry is malformed (a flat name and its reason): "
					+ (safeProtocolText(name) ? name : std::string{});
				return false;
			}
			dst[lower(flat)] = map[name].asString();
		}
	}
	// A name both required and excluded is a contradiction; an absent note for a name no set
	// requires describes nothing the report could check.
	for (const auto& [set, names] : out.sets)
	{
		for (const auto& name : names)
		{
			if (out.excluded.contains(name))
			{
				error = "startup manifest both requires (" + set + ") and excludes " + name;
				return false;
			}
		}
	}
	for (const auto& [name, where] : out.absent)
	{
		bool required = false;
		for (const auto& [set, names] : out.sets)
			required = required || std::binary_search(names.begin(), names.end(), name);
		if (!required)
		{
			error = "startup manifest notes " + name + " as absent, but no set requires it";
			return false;
		}
	}
	if (!root["inputs"].isNull() && !root["inputs"].isObject())
	{
		error = "startup manifest inputs is not an object";
		return false;
	}
	// The DLL's rule (cp_core.cpp SafeInputPath / IsSafeFlatName), so every row the report lists is
	// one the DLL can read: '/'-separated names, each non-empty, at most 255 characters, not "." or
	// "..", no control character or \ : * ? " < > |, not ending in "." or " "; a LocalState input and
	// a server table are a single name.
	const auto safePath = [](const std::string& p, bool flat) {
		if (p.empty() || p.size() > 260 || (flat && p.find('/') != std::string::npos))
			return false;
		size_t start = 0;
		while (start <= p.size())
		{
			const size_t end = std::min(p.find('/', start), p.size());
			const std::string segment = p.substr(start, end - start);
			if (segment.empty() || segment.size() > 255 || segment == "." || segment == ".."
				|| segment.back() == '.' || segment.back() == ' '
				|| segment.find_first_of("\\:*?\"<>|") != std::string::npos)
				return false;
			for (const unsigned char c : segment)
				if (c < 0x20 || c == 0x7F)
					return false;
			start = end + 1;
		}
		return true;
	};
	const auto lowerHex64 = [](const std::string& h) {
		return h.size() == 64 && h.find_first_not_of("0123456789abcdef") == std::string::npos;
	};
	// An entry is exactly {root, path, sha256}, all strings.
	static const std::set<std::string> kInputKeys{ "root", "path", "sha256" };
	const auto inputShape = [](const Json::Value& v) {
		bool good = v.isObject() && v.size() == kInputKeys.size();
		for (const auto& key : good ? v.getMemberNames() : std::vector<std::string>{})
			good = good && kInputKeys.contains(key) && v[key].isString();
		return good;
	};
	// inputs.client_files: the installed client's own files the sets were derived from.  Each
	// entry names its root, a path that cannot leave that root, and the SHA-256 of the bytes.
	const auto& clientFiles = root["inputs"].isObject() ? root["inputs"]["client_files"] : Json::Value();
	if (!clientFiles.isNull())
	{
		if (!clientFiles.isArray())
		{
			error = "startup manifest inputs.client_files is not an array";
			return false;
		}
		std::set<std::string> seen;
		for (const auto& v : clientFiles)
		{
			bool good = inputShape(v);
			StartupManifest::ClientInput input;
			if (good)
			{
				input.root = v["root"].asString();
				input.path = v["path"].asString();
				input.digest = "sha256:" + v["sha256"].asString();
				good = (input.root == "localstate" || input.root == "package")
					&& safePath(input.path, input.root == "localstate") && lowerHex64(v["sha256"].asString());
			}
			if (!good || !seen.insert(input.root + "/" + lower(input.path)).second)
			{
				error = "startup manifest inputs.client_files entry is malformed or repeated (root localstate or "
					"package, a path inside it, a lower-case SHA-256)";
				return false;
			}
			out.clientInputs.push_back(std::move(input));
		}
	}
	// inputs.tables records what each derivation read (scripts/startup_coverage.py): every LocalState
	// part it names -- a version table's own, its `also` sources', its joined tables' -- must be bound in
	// inputs.client_files, or the DLL would never compare the client's copy of it (Session 19: an
	// omitted client binding is refused, as an omitted server table is).
	const auto& tables = root["inputs"].isObject() ? root["inputs"]["tables"] : Json::Value();
	if (!tables.isNull())
	{
		if (!tables.isObject())
		{
			error = "startup manifest inputs.tables is not an object";
			return false;
		}
		std::set<std::string> bound;
		for (const auto& input : out.clientInputs)
			if (input.root == "localstate")
				bound.insert(lower(input.path));
		std::vector<const Json::Value*> todo;
		for (const auto& name : tables.getMemberNames())
			todo.push_back(&tables[name]);
		while (!todo.empty())
		{
			const Json::Value& identity = *todo.back();
			todo.pop_back();
			if (!identity.isObject())
				continue;
			for (const auto& part : identity["parts"])
			{
				if (part.isString() && !bound.contains(lower(part.asString())))
				{
					error = "startup manifest inputs.tables records reading " + oneLine(part.asString())
						+ ", which inputs.client_files does not bind";
					return false;
				}
			}
			for (const char* key : { "also", "joined" })
				for (const auto& more : identity[key])
					todo.push_back(&more);
		}
	}
	// inputs.server_files (Session 18 correction): the server's response tables the derived lists
	// read, each a file directly under the server's mst_root with the SHA-256 of its bytes.  start()
	// binds them to the tables this server actually serves (bindServerTables).
	const auto& serverFiles = root["inputs"].isObject() ? root["inputs"]["server_files"] : Json::Value();
	if (!serverFiles.isNull())
	{
		if (!serverFiles.isArray())
		{
			error = "startup manifest inputs.server_files is not an array";
			return false;
		}
		std::set<std::string> seen;
		for (const auto& v : serverFiles)
		{
			bool good = inputShape(v);
			StartupManifest::ServerInput input;
			if (good)
			{
				input.root = v["root"].asString();
				input.path = v["path"].asString();
				input.digest = "sha256:" + v["sha256"].asString();
				good = (input.root == "mst" || input.root == "served") && safePath(input.path, true)
					&& lowerHex64(v["sha256"].asString());
			}
			if (!good || !seen.insert(lower(serverInputName(input.root, input.path))).second)
			{
				error = "startup manifest inputs.server_files entry is malformed or repeated (root mst or served, a "
					"file name or response key, a lower-case SHA-256)";
				return false;
			}
			out.serverInputs.push_back(std::move(input));
		}
	}
	out.loaded = true;
	return true;
}

/*!
* Why @p manifest is not bound to the response tables this server serves; empty when it is
* (Session 18 correction).  Every inputs.server_files entry must name a table a response carries
* to a startup list, with the digest of the bytes the cache parsed, and every such table must be
* named -- a manifest derived from other rows describes other requirements.  A manifest naming
* none is unbound (an old one): never complete.  Names compare case-insensitively (one file on
* Windows); the problems are listed in name order.  scripts/startup_coverage.py server_binding
* gives the same text for the same inputs.
*/
std::string bindServerTables(const StartupManifest& manifest, const std::vector<ServedTable>& served)
{
	if (served.empty())
		return "the server recorded none of the response tables the startup lists read: there is nothing to bind "
			"the manifest to";
	if (manifest.serverInputs.empty())
		return "the manifest names none of the server's response tables (inputs.server_files): it cannot be bound "
			"to what this server's responses serve";
	const auto abbreviated = [](const std::string& digest) { return digest.substr(0, 7 + 16) + "..."; };
	std::vector<std::pair<std::string, std::string>> problems;   // (lower name, text)
	std::set<std::string> named;
	for (const auto& input : manifest.serverInputs)
	{
		const std::string shown = serverInputName(input.root, input.path);
		const std::string key = lower(shown);
		named.insert(key);
		const auto table = std::find_if(served.begin(), served.end(),
			[&key](const ServedTable& t) { return lower(serverInputName(t.root, t.path)) == key; });
		if (table == served.end())
			problems.emplace_back(key, shown + " is not a table any response serves to a startup list");
		else if (table->digest != input.digest)
			problems.emplace_back(key, shown + ": derived from " + abbreviated(input.digest) + ", served "
				+ abbreviated(table->digest));
	}
	for (const auto& table : served)
	{
		const std::string shown = serverInputName(table.root, table.path);
		if (!named.contains(lower(shown)))
			problems.emplace_back(lower(shown), shown + ": not named (served " + abbreviated(table.digest) + ")");
	}
	if (problems.empty())
		return {};
	std::sort(problems.begin(), problems.end());
	std::string reason = "the manifest does not match the response tables this server serves: ";
	for (size_t i = 0; i < problems.size(); ++i)
		reason += (i ? "; " : "") + problems[i].second;
	return reason;
}

#ifdef _WIN32
std::wstring finalPathOf(HANDLE h)
{
	wchar_t buffer[1024];
	const DWORD n = GetFinalPathNameByHandleW(h, buffer, 1024, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	if (n == 0 || n >= 1024)
		return {};
	std::wstring s(buffer, n);
	if (s.rfind(L"\\\\?\\", 0) == 0)
		s = s.substr(4);
	return s;
}

bool isReparse(const fs::path& p)
{
	const DWORD attrs = GetFileAttributesW(p.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT);
}

bool startsWithI(const std::wstring& s, const std::wstring& prefix)
{
	return s.size() >= prefix.size()
		&& CompareStringOrdinal(s.c_str(), static_cast<int>(prefix.size()),
			prefix.c_str(), static_cast<int>(prefix.size()), TRUE) == CSTR_EQUAL;
}
#endif

/*!
* One enumeration pass.  Reparse points (symlinks, junctions) are counted and
* skipped -- never followed -- so nothing outside the root is indexed.
*/
struct ScanRecord
{
	uint64_t size = 0;
	int64_t mtime = 0;
};

#ifdef _WIN32
void walk(const fs::path& root, const fs::path& start,
	std::unordered_map<std::string, ScanRecord>& out, uint64_t& skippedReparse, uint64_t& skippedNames)
{
	// FindFirstFileExW hands back size, write time and attributes with every
	// name, so the 76,000-file tree costs one enumeration per folder instead of
	// several metadata calls per file (15.9 s -> 3.7 s measured on the full
	// 75,931-file tree, warm OS cache, 2026-10-03).
	// The write time is the raw FILETIME, which is what MSVC's
	// fs::last_write_time().time_since_epoch().count() returns as well.
	const std::wstring rootW = root.wstring();
	std::vector<std::wstring> stack{ start.wstring() };
	while (!stack.empty())
	{
		const std::wstring dir = std::move(stack.back());
		stack.pop_back();
		WIN32_FIND_DATAW fd;
		HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch,
			nullptr, FIND_FIRST_EX_LARGE_FETCH);
		if (h == INVALID_HANDLE_VALUE)
			continue;
		do
		{
			const std::wstring name = fd.cFileName;
			if (name == L"." || name == L"..")
				continue;
			const std::wstring full = dir + L"\\" + name;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			{
				++skippedReparse;   // never followed: symlinks and junctions can leave the root
				continue;
			}
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				stack.push_back(full);
				continue;
			}
			const auto rel = toUtf8(fs::path(full.substr(rootW.size() + 1)));
			if (!safeProtocolText(rel) || rel.find("..") != std::string::npos)
			{
				++skippedNames;
				continue;
			}
			ScanRecord rec;
			rec.size = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
			rec.mtime = static_cast<int64_t>((static_cast<uint64_t>(fd.ftLastWriteTime.dwHighDateTime) << 32)
				| fd.ftLastWriteTime.dwLowDateTime);
			out[rel] = rec;
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}
}
#else
void walk(const fs::path& root, const fs::path& start,
	std::unordered_map<std::string, ScanRecord>& out, uint64_t& skippedReparse, uint64_t& skippedNames)
{
	std::error_code ec;
	fs::recursive_directory_iterator it(start, fs::directory_options::skip_permission_denied, ec);
	if (ec)
		return;
	for (; it != fs::recursive_directory_iterator(); it.increment(ec))
	{
		if (ec)
			break;
		const auto& entry = *it;
		std::error_code stEc;
		const auto st = entry.symlink_status(stEc);
		bool reparse = !stEc && fs::is_symlink(st);
#ifdef _WIN32
		reparse = reparse || isReparse(entry.path());
#endif
		if (reparse)
		{
			++skippedReparse;
			if (entry.is_directory(stEc))
				it.disable_recursion_pending();
			continue;
		}
		if (!entry.is_regular_file(stEc) || stEc)
			continue;
		const auto rel = toUtf8(entry.path().lexically_relative(root));
		if (!safeProtocolText(rel) || rel.find("..") != std::string::npos)
		{
			++skippedNames;
			continue;
		}
		ScanRecord rec;
		rec.size = entry.file_size(stEc);
		if (stEc)
			continue;
		const auto t = entry.last_write_time(stEc);
		rec.mtime = stEc ? 0 : static_cast<int64_t>(t.time_since_epoch().count());
		out[rel] = rec;
	}
}
#endif

} // namespace

ContentBroker& ContentBroker::instance()
{
	static ContentBroker broker;
	return broker;
}

void ContentBroker::start(const fs::path& documentRoot, const fs::path& systemRoot, std::vector<ServedTable> served)
{
	m_epoch = randomEpoch();
	m_served = std::move(served);
	m_serverBinding.clear();
	std::error_code ec;
	const auto policyFile = fs::absolute(systemRoot, ec) / "content_provider.json";
	std::string error;
	if (!loadPolicy(policyFile, m_policy, error))
	{
		m_reason = error;
		LOG_INFO << "content broker: dormant (" << error << ")";
		return;
	}
	{
		std::ifstream in(policyFile, std::ios::binary);
		std::stringstream ss;
		ss << in.rdbuf();
		m_policyDigest = sha256Tag(ss.str());
	}
	// The startup coverage manifest is optional: without it every coverage
	// report says incomplete, and nothing else about the broker changes.
	std::string manifestError;
	const auto manifestFile = fs::absolute(systemRoot, ec) / "content_startup.json";
	if (loadManifest(manifestFile, m_manifest, manifestError))
	{
		size_t names = 0;
		for (const auto& [set, list] : m_manifest.sets)
			names += list.size();
		LOG_INFO << "content broker: startup manifest " << m_manifest.digest.substr(0, 7 + 16) << " for client build "
		         << m_manifest.build << ": " << m_manifest.sets.size() << " set(s), " << names << " name(s), "
		         << m_manifest.unknown.size() << " unknown list(s), " << m_manifest.excluded.size() << " excluded, "
		         << m_manifest.absent.size() << " absent from every source, " << m_manifest.clientInputs.size()
		         << " client input file(s), " << m_manifest.serverInputs.size() << " server table(s)";
		// Both sides are fixed for the life of the process -- the manifest is read once, here, and the
		// cache parsed the tables once, before -- so the binding is decided once.
		m_serverBinding = bindServerTables(m_manifest, m_served);
		for (const auto& table : m_served)
			LOG_INFO << "content broker: serves response table " << serverInputName(table.root, table.path) << " ("
			         << table.carriedBy << ") " << table.digest;
		if (m_serverBinding.empty())
			LOG_INFO << "content broker: the startup manifest is bound to the " << m_served.size()
			         << " response table(s) this server serves";
		else
			LOG_WARN << "content broker: " << m_serverBinding << "; coverage reports stay incomplete";
	}
	else
	{
		m_manifest.error = manifestError;
		LOG_INFO << "content broker: no usable startup manifest (" << manifestError
		         << "); coverage reports stay incomplete";
	}
	if (!m_policy.enabled)
	{
		m_reason = "disabled by " + policyFile.string();
		LOG_INFO << "content broker: dormant (" << m_reason << ")";
		return;
	}

	fs::path root = fs::absolute(documentRoot, ec) / "content";
	if (!m_policy.root.empty())
	{
		const fs::path configured = fromUtf8(m_policy.root);
		root = configured.is_absolute() ? configured : fs::absolute(documentRoot, ec) / configured;
	}
	m_root = fs::weakly_canonical(root, ec);
	if (ec || !fs::is_directory(m_root, ec))
	{
		m_reason = "content root missing: " + root.string();
		LOG_WARN << "content broker: dormant (" << m_reason << ")";
		return;
	}
#ifdef _WIN32
	HANDLE h = CreateFileW(m_root.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (h != INVALID_HANDLE_VALUE)
	{
		m_rootFinal = finalPathOf(h);
		CloseHandle(h);
	}
	if (m_rootFinal.empty())
	{
		m_reason = "cannot resolve the final path of the content root";
		LOG_WARN << "content broker: dormant (" << m_reason << ")";
		return;
	}
	if (m_rootFinal.back() != L'\\')
		m_rootFinal.push_back(L'\\');
	m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
#endif

	m_reason = "building index";
	m_enabled = true;
	m_stop = false;
	m_builder = std::thread([this] { buildThread(); });
}

void ContentBroker::stop()
{
	m_stop = true;
#ifdef _WIN32
	if (m_stopEvent)
		SetEvent(static_cast<HANDLE>(m_stopEvent));
#endif
	if (m_builder.joinable())
		m_builder.join();
	if (m_watcher.joinable())
		m_watcher.join();
#ifdef _WIN32
	if (m_stopEvent)
	{
		CloseHandle(static_cast<HANDLE>(m_stopEvent));
		m_stopEvent = nullptr;
	}
#endif
}

bool ContentBroker::inScope(const std::string& rel) const
{
	// globMatch is case-insensitive, and so is the exact-path set.
	if (m_policy.scopeExact.contains(lower(rel)))
		return true;
	for (const auto& pattern : m_policy.scopeGlobs)
		if (globMatch(pattern.c_str(), rel.c_str()))
			return true;
	return false;
}

bool ContentBroker::allowedName(const std::string& basename) const
{
	const auto ext = extensionOf(basename);
	if (std::find(m_policy.extensions.begin(), m_policy.extensions.end(), ext) == m_policy.extensions.end())
		return false;
	for (const auto& pattern : m_policy.exclude)
		if (globMatch(pattern.c_str(), basename.c_str()))
			return false;
	return true;
}

std::string ContentBroker::hashFile(const fs::path& p, uint64_t& size, int64_t& mtime, std::string& error) const
{
	CryptoPP::SHA256 sha;
	std::vector<char> buffer(1 << 16);
	uint64_t total = 0;
#ifdef _WIN32
	// Shares everything: a hash never blocks a writer.  An atomic replace can
	// complete while the old file is being hashed -- its own notification
	// brings the new file here next -- but the open itself can collide with
	// the replace for a moment, so that is retried briefly (as in read()).
	HANDLE h = INVALID_HANDLE_VALUE;
	for (int attempt = 0; attempt < 6; ++attempt)
	{
		h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
			OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (h != INVALID_HANDLE_VALUE)
			break;
		const DWORD err = GetLastError();
		if (err != ERROR_SHARING_VIOLATION && err != ERROR_ACCESS_DENIED && err != ERROR_LOCK_VIOLATION)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(40));
	}
	if (h == INVALID_HANDLE_VALUE)
	{
		error = "open failed (" + std::to_string(GetLastError()) + ")";
		return {};
	}
	bool ok = true;
	for (;;)
	{
		DWORD got = 0;
		if (!ReadFile(h, buffer.data(), static_cast<DWORD>(buffer.size()), &got, nullptr))
		{
			ok = false;
			break;
		}
		if (got == 0)
			break;
		sha.Update(reinterpret_cast<const CryptoPP::byte*>(buffer.data()), got);
		total += got;
	}
	BY_HANDLE_FILE_INFORMATION info{};
	ok = ok && GetFileInformationByHandle(h, &info) != FALSE;
	CloseHandle(h);
	if (!ok)
	{
		error = "read failed";
		return {};
	}
	// The raw FILETIME, like walk() and fs::last_write_time on MSVC.
	mtime = static_cast<int64_t>((static_cast<uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32)
		| info.ftLastWriteTime.dwLowDateTime);
#else
	std::ifstream in(p, std::ios::binary);
	if (!in)
	{
		error = "open failed";
		return {};
	}
	while (in)
	{
		in.read(buffer.data(), buffer.size());
		const auto n = in.gcount();
		if (n > 0)
		{
			sha.Update(reinterpret_cast<const CryptoPP::byte*>(buffer.data()), static_cast<size_t>(n));
			total += static_cast<uint64_t>(n);
		}
	}
	std::error_code ec;
	const auto t = fs::last_write_time(p, ec);
	mtime = ec ? 0 : static_cast<int64_t>(t.time_since_epoch().count());
#endif
	std::string digest;
	CryptoPP::byte raw[CryptoPP::SHA256::DIGESTSIZE];
	sha.Final(raw);
	CryptoPP::HexEncoder enc(new CryptoPP::StringSink(digest), false);
	enc.Put(raw, sizeof raw);
	enc.MessageEnd();
	size = total;
	return "sha256:" + digest;
}

void ContentBroker::buildThread()
{
	std::lock_guard writer(m_writer);
	const auto started = std::chrono::steady_clock::now();
	std::unordered_map<std::string, ScanRecord> scan;
	uint64_t skippedReparse = 0, skippedNames = 0;
	walk(m_root, m_root, scan, skippedReparse, skippedNames);
	if (m_stop)
		return;
	{
		std::unique_lock lock(m_lock);
		m_files.clear();
		for (const auto& [rel, rec] : scan)
		{
			FileEntry e;
			e.rel = rel;
			e.size = rec.size;
			e.mtime = rec.mtime;
			e.gen = 1;
			m_files.emplace(rel, std::move(e));
		}
		m_skippedReparse = skippedReparse;
		m_generation = 1;
	}
	recomputeOwned(1);
	{
		std::shared_lock lock(m_lock);
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - started).count();
		LOG_INFO << "content broker: indexed " << m_files.size() << " file(s) in " << ms << " ms; "
		         << m_owned.size() << " owned, " << m_collisions.size() << " collision(s), "
		         << skippedReparse << " reparse point(s) and " << skippedNames << " unsafe name(s) skipped";
		for (const auto& [name, rels] : m_collisions)
		{
			std::string list;
			for (const auto& r : rels)
				list += (list.empty() ? "" : ", ") + r;
			LOG_WARN << "content broker: collision for " << name << " (not served without an alias): " << list;
		}
	}
	{
		std::unique_lock lock(m_lock);
		m_reason.clear();
	}
	m_ready = true;
	m_watcher = std::thread([this] { watchThread(); });
}

void ContentBroker::recomputeOwned(uint64_t newGen)
{
	struct Group
	{
		std::vector<std::string> rels;     //!< every copy of the basename, sorted
		std::vector<std::string> scoped;   //!< the copies inside the authored scope
	};
	std::unordered_map<std::string, Group> groups;
	{
		std::shared_lock lock(m_lock);
		for (const auto& [rel, e] : m_files)
			groups[lower(basenameOf(rel))].rels.push_back(rel);
	}
	// Only an allowed name with a copy in scope (or an authored alias, which
	// is reported when it points outside the scope) can be owned.  The
	// authored globs are matched once per file, here: on the full tree this
	// is most of the work.
	for (auto it = groups.begin(); it != groups.end();)
	{
		auto& g = it->second;
		if (!allowedName(basenameOf(g.rels.front())))
		{
			it = groups.erase(it);
			continue;
		}
		std::sort(g.rels.begin(), g.rels.end());
		for (const auto& r : g.rels)
			if (inScope(r))
				g.scoped.push_back(r);
		if (g.scoped.empty() && !m_policy.aliases.contains(it->first))
			it = groups.erase(it);
		else
			++it;
	}

	// Identities are computed before anything is offered: the copy a name is
	// served from (its digest is what the index publishes and what a pinned
	// client read is checked against), and every copy of a scoped basename
	// that exists more than once, so byte-identical duplicates are told apart
	// from real collisions.
	std::vector<std::string> toHash;
	{
		std::shared_lock lock(m_lock);
		const auto unhashed = [&](const std::string& r) {
			const auto it = m_files.find(r);
			return it != m_files.end() && it->second.digest.empty();
		};
		for (const auto& [name, g] : groups)
		{
			if (const auto alias = m_policy.aliases.find(name); alias != m_policy.aliases.end())
			{
				if (unhashed(alias->second))
					toHash.push_back(alias->second);
				continue;
			}
			for (const auto& r : g.rels)
				if (unhashed(r))
					toHash.push_back(r);
		}
	}
	for (const auto& rel : toHash)
	{
		uint64_t size = 0;
		int64_t mtime = 0;
		std::string error;
		const auto digest = hashFile(m_root / fromUtf8(rel), size, mtime, error);
		std::unique_lock lock(m_lock);
		const auto it = m_files.find(rel);
		if (it != m_files.end() && !digest.empty() && it->second.size == size && it->second.mtime == mtime)
			it->second.digest = digest;
	}

	std::unordered_map<std::string, OwnedEntry> owned;
	std::map<std::string, std::vector<std::string>> collisions;
	std::map<std::string, std::string> pending;   //!< lower basename -> why it has no identity yet
	std::unique_lock lock(m_lock);
	for (const auto& [name, g] : groups)
	{
		const auto& rels = g.rels;
		std::string target;
		if (const auto alias = m_policy.aliases.find(name); alias != m_policy.aliases.end())
		{
			if (!m_files.contains(alias->second) || !inScope(alias->second))
			{
				collisions[name] = { "alias target missing or out of scope: " + alias->second };
				continue;
			}
			target = alias->second;
		}
		else
		{
			if (rels.size() > 1)
			{
				const auto digestOf = [&](const std::string& r) {
					const auto it = m_files.find(r);
					return it == m_files.end() ? std::string{} : it->second.digest;
				};
				const auto first = digestOf(rels.front());
				const bool identical = !first.empty() && std::all_of(rels.begin(), rels.end(),
					[&](const auto& r) { return digestOf(r) == first; });
				if (!identical)
				{
					collisions[name] = rels;
					continue;
				}
			}
			target = g.scoped.front();
		}
		const auto fileIt = m_files.find(target);
		if (fileIt == m_files.end())
			continue;
		const auto& file = fileIt->second;
		if (file.digest.empty())
		{
			// It changed while it was being hashed (or could not be read): a
			// name is only offered together with the identity clients pin, so
			// it waits for the watcher's next look at the file.
			pending[name] = "identity pending for " + target;
			continue;
		}
		OwnedEntry e;
		e.basename = basenameOf(target);
		e.rel = target;
		e.size = file.size;
		e.digest = file.digest;
		const auto previous = m_owned.find(name);
		// A name that starts (or resumes) being served, or now maps to another
		// file, is new to every client: it gets the current generation so a
		// delta carries it.
		e.gen = (previous == m_owned.end() || previous->second.rel != target) ? newGen : file.gen;
		owned.emplace(name, std::move(e));
	}

	// Bundles are all or nothing.  A member that cannot be served (missing,
	// ambiguous, out of scope) makes every member unavailable, so the client
	// takes its own missing-asset path instead of drawing half a unit.
	// Bundles can share members (generated unit families: elemental variants
	// drawn with one frame layout), and withdrawing one bundle takes the shared
	// member from every other bundle that has it -- including bundles already
	// judged complete earlier in this pass.  So the pass repeats until no
	// bundle withdraws anything more.
	std::map<std::string, std::pair<uint64_t, std::string>> unavailable;
	for (bool withdrew = true; withdrew;)
	{
		withdrew = false;
		for (const auto& [bundle, members] : m_policy.bundles)
		{
			std::string missing;
			bool anyOwned = false;
			for (const auto& m : members)
			{
				if (owned.contains(m))
					anyOwned = true;
				else
					missing += (missing.empty() ? "" : ", ") + m;
			}
			if (missing.empty() || !anyOwned)
				continue;
			const auto reason = "bundle " + bundle + " incomplete (missing " + missing + ")";
			for (const auto& m : members)
			{
				owned.erase(m);
				const auto prev = m_unavailable.find(m);
				unavailable.try_emplace(m, prev != m_unavailable.end() ? prev->second.first : newGen, reason);
			}
			withdrew = true;
		}
	}
	// A bundle whose members were all withdrawn by others still reports every
	// member unavailable (a missing member that no earlier bundle named too).
	for (const auto& [bundle, members] : m_policy.bundles)
	{
		bool complete = true;
		for (const auto& m : members)
			complete = complete && owned.contains(m);
		if (complete)
			continue;
		for (const auto& m : members)
		{
			if (unavailable.contains(m))
				continue;
			const auto prev = m_unavailable.find(m);
			unavailable.emplace(m, std::make_pair(prev != m_unavailable.end() ? prev->second.first : newGen,
				"bundle " + bundle + " incomplete"));
		}
	}
	// A name without an identity is reported unavailable rather than left
	// unowned, so the client does not fall back to its own stale copy.
	for (const auto& [name, reason] : pending)
	{
		if (unavailable.contains(name))
			continue;
		const auto prev = m_unavailable.find(name);
		unavailable[name] = { prev != m_unavailable.end() ? prev->second.first : newGen, reason };
	}
	for (const auto& [name, info] : unavailable)
		if (!m_unavailable.contains(name))
			LOG_WARN << "content broker: " << name << " unavailable: " << info.second;
	for (const auto& [name, prev] : m_owned)
		if (!owned.contains(name))
			m_tombstones[name] = newGen;
	for (const auto& [name, e] : owned)
		m_tombstones.erase(name);
	m_owned = std::move(owned);
	m_collisions = std::move(collisions);
	m_unavailable = std::move(unavailable);
	// Published with the sets it describes (Session 16): a reader holding the
	// shared lock -- the index, the coverage report -- never pairs the new
	// owned set with the previous generation number.
	if (newGen > m_generation.load())
		m_generation = newGen;
}

std::vector<std::string> ContentBroker::applyChanged(const std::vector<std::string>& rels, const char* why)
{
	std::lock_guard writer(m_writer);
	const uint64_t newGen = m_generation.load() + 1;
	size_t changed = 0;
	std::vector<std::string> unexamined;
	for (const auto& rel : rels)
	{
		const auto path = m_root / fromUtf8(rel);
		// ONLY A NAME THAT IS DEFINITELY GONE (or no longer a plain file) IS
		// WITHDRAWN.  During an atomic replace the old file is delete-pending
		// for a moment, and examining it then fails with access denied: that
		// used to withdraw the name -- 404 not-owned until the next quiet
		// period, which under a writer that keeps replacing is the rest of the
		// burst (test_content_broker_wire.py, intermittently).  A name that
		// cannot be examined keeps its record and is looked at again.
		std::error_code ec;
		const auto st = fs::status(path, ec);
		if (ec && st.type() != fs::file_type::not_found)
		{
			LOG_WARN << "content broker: cannot examine " << rel << " (" << ec.message() << "); keeping its record";
			unexamined.push_back(rel);
			continue;
		}
		const bool exists = st.type() == fs::file_type::regular
#ifdef _WIN32
			&& !isReparse(path)
#endif
			;
		if (!exists)
		{
			std::unique_lock lock(m_lock);
			if (m_files.erase(rel))
				++changed;
			continue;
		}
		std::error_code timeEc;
		const auto size = fs::file_size(path, ec);
		const auto t = fs::last_write_time(path, timeEc);
		if (ec || timeEc)
		{
			LOG_WARN << "content broker: cannot examine " << rel << " (" << (ec ? ec : timeEc).message()
				<< "); keeping its record";
			unexamined.push_back(rel);
			continue;
		}
		const int64_t mtime = static_cast<int64_t>(t.time_since_epoch().count());

		bool owned = false;
		std::string knownDigest;
		{
			std::shared_lock lock(m_lock);
			owned = m_owned.contains(lower(basenameOf(rel)));
			if (const auto it = m_files.find(rel); it != m_files.end())
				knownDigest = it->second.digest;
		}
		// Size and mtime alone are not proof of equality: anything with an
		// identity (an owned name's copy, a compared duplicate) or that may
		// become owned is re-hashed, so a same-size edit is caught and a mere
		// touch is not counted as a change.
		std::string digest;
		uint64_t hashedSize = size;
		int64_t hashedMtime = mtime;
		if (owned || !knownDigest.empty() || (inScope(rel) && allowedName(basenameOf(rel))))
		{
			std::string error;
			digest = hashFile(path, hashedSize, hashedMtime, error);
			if (digest.empty() && !knownDigest.empty())
			{
				// Unreadable this instant (held exclusively by another
				// process): keep the identity on record instead of
				// withdrawing the name.  Reads verify their own bytes, and the
				// writer's notification -- or the watcher's second look --
				// brings the file back here.
				LOG_WARN << "content broker: cannot hash " << rel << " (" << error << "); keeping its recorded identity";
				unexamined.push_back(rel);
				continue;
			}
		}

		std::unique_lock lock(m_lock);
		auto it = m_files.find(rel);
		if (it == m_files.end())
		{
			FileEntry e;
			e.rel = rel;
			e.size = hashedSize;
			e.mtime = hashedMtime;
			e.digest = digest;
			e.gen = newGen;
			m_files.emplace(rel, std::move(e));
			++changed;
			continue;
		}
		auto& e = it->second;
		bool differs = e.size != hashedSize;
		if (!digest.empty())
			differs = differs || e.digest.empty() || e.digest != digest;
		else
			differs = differs || e.mtime != hashedMtime;
		e.size = hashedSize;
		e.mtime = hashedMtime;
		e.digest = digest;
		if (differs)
		{
			e.gen = newGen;
			++changed;
		}
	}
	if (changed)
	{
		recomputeOwned(newGen);
		m_generation = newGen;
		LOG_INFO << "content broker: generation " << newGen << " (" << changed << " change(s), " << why << ")";
	}
	return unexamined;
}

std::vector<std::string> ContentBroker::rescan(const char* why)
{
	std::lock_guard writer(m_writer);
	std::unordered_map<std::string, ScanRecord> scan;
	uint64_t skippedReparse = 0, skippedNames = 0;
	walk(m_root, m_root, scan, skippedReparse, skippedNames);
	std::vector<std::string> candidates;
	{
		std::shared_lock lock(m_lock);
		for (const auto& [rel, rec] : scan)
		{
			const auto it = m_files.find(rel);
			// Unchanged size+mtime is re-checked by digest for every file whose
			// digest is known (i.e. one a client has fetched): an overflow may
			// have hidden a same-size edit made with a preserved timestamp.
			if (it == m_files.end() || it->second.size != rec.size || it->second.mtime != rec.mtime
				|| !it->second.digest.empty())
				candidates.push_back(rel);
		}
		for (const auto& [rel, e] : m_files)
			if (!scan.contains(rel))
				candidates.push_back(rel);
	}
	{
		std::unique_lock lock(m_lock);
		m_skippedReparse = skippedReparse;
		++m_rescans;
	}
	return applyChanged(candidates, why);
}

void ContentBroker::watchThread()
{
#ifdef _WIN32
	HANDLE dir = CreateFileW(m_root.c_str(), FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
	if (dir == INVALID_HANDLE_VALUE)
	{
		LOG_WARN << "content broker: cannot watch " << m_root.string() << "; edits are seen only by rescans";
		while (!m_stop)
		{
			if (WaitForSingleObject(static_cast<HANDLE>(m_stopEvent), 5000) == WAIT_OBJECT_0)
				break;
			rescan("periodic rescan (no watcher)");
		}
		return;
	}
	std::vector<DWORD> buffer(kWatchBufferBytes / sizeof(DWORD));
	OVERLAPPED ov{};
	ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	std::set<std::string> pending;
	std::map<std::string, int> secondLooks;   //!< name applyChanged could not examine -> looks so far
	bool needRescan = false;
	bool readPending = false;
	const DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
		| FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;

	const auto flush = [&] {
		std::vector<std::string> unexamined;
		if (needRescan)
		{
			++m_overflows;
			unexamined = rescan("watcher overflow");
		}
		else if (!pending.empty())
		{
			// A name that is (or was) a directory needs its whole subtree.
			bool subtree = false;
			std::vector<std::string> rels(pending.begin(), pending.end());
			for (const auto& rel : rels)
			{
				std::error_code ec;
				if (fs::is_directory(m_root / fromUtf8(rel), ec))
				{
					subtree = true;
					break;
				}
				std::shared_lock lock(m_lock);
				if (!m_files.contains(rel))
				{
					const auto prefix = rel + "/";
					for (const auto& [known, e] : m_files)
						if (known.starts_with(prefix))
						{
							subtree = true;
							break;
						}
				}
				if (subtree)
					break;
			}
			if (subtree)
				unexamined = rescan("directory change");
			else
				unexamined = applyChanged(rels, "watcher");
		}
		pending.clear();
		needRescan = false;
		// A name that could not be examined (mid-replace) is looked at again
		// after the next quiet period, a bounded number of times (its own
		// notifications usually bring it back sooner); past that its record
		// stays until it changes again.
		std::map<std::string, int> looks;
		for (const auto& rel : unexamined)
		{
			const auto it = secondLooks.find(rel);
			const int n = it == secondLooks.end() ? 1 : it->second + 1;
			if (n > kSecondLooks)
			{
				LOG_WARN << "content broker: " << rel << " still cannot be examined; its record stays until it changes";
				continue;
			}
			looks[rel] = n;
			pending.insert(rel);
		}
		secondLooks = std::move(looks);
	};

	while (!m_stop)
	{
		if (!readPending)
		{
			ResetEvent(ov.hEvent);
			if (!ReadDirectoryChangesW(dir, buffer.data(), static_cast<DWORD>(kWatchBufferBytes), TRUE,
				filter, nullptr, &ov, nullptr))
			{
				LOG_WARN << "content broker: ReadDirectoryChangesW failed (" << GetLastError() << "); rescanning";
				rescan("watcher error");
				if (WaitForSingleObject(static_cast<HANDLE>(m_stopEvent), 1000) == WAIT_OBJECT_0)
					break;
				continue;
			}
			readPending = true;
		}
		HANDLE waits[2] = { ov.hEvent, static_cast<HANDLE>(m_stopEvent) };
		const bool havePending = !pending.empty() || needRescan;
		const DWORD w = WaitForMultipleObjects(2, waits, FALSE,
			havePending ? static_cast<DWORD>(kDebounce.count()) : INFINITE);
		if (w == WAIT_OBJECT_0 + 1)
			break;
		if (w == WAIT_TIMEOUT)
		{
			// Quiet for the debounce period: apply the batch as ONE generation
			// so a multi-file publication is adopted together.  The read stays
			// outstanding, so nothing that happens meanwhile is lost.
			flush();
			continue;
		}
		readPending = false;
		DWORD bytes = 0;
		if (!GetOverlappedResult(dir, &ov, &bytes, FALSE))
		{
			if (GetLastError() == ERROR_NOTIFY_ENUM_DIR)
				needRescan = true;
			continue;
		}
		if (bytes == 0)
		{
			// The buffer overflowed: individual events were lost.
			needRescan = true;
			continue;
		}
		auto* base = reinterpret_cast<unsigned char*>(buffer.data());
		for (auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(base); ; )
		{
			std::wstring wname(info->FileName, info->FileNameLength / sizeof(wchar_t));
			const auto rel = toUtf8(fs::path(wname));
			if (safeProtocolText(rel))
				pending.insert(rel);
			if (!info->NextEntryOffset)
				break;
			info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<unsigned char*>(info) + info->NextEntryOffset);
		}
	}
	if (readPending)
	{
		CancelIo(dir);
		DWORD ignored = 0;
		GetOverlappedResult(dir, &ov, &ignored, TRUE);
	}
	CloseHandle(ov.hEvent);
	CloseHandle(dir);
#else
	while (!m_stop)
	{
		std::this_thread::sleep_for(std::chrono::seconds(5));
		if (!m_stop)
			rescan("periodic rescan");
	}
#endif
}

std::string ContentBroker::statusText() const
{
	std::ostringstream o;
	o << kProtocol << "\n"
	  << "enabled " << (m_enabled ? 1 : 0) << "\n"
	  << "ready " << (m_ready ? 1 : 0) << "\n"
	  << "epoch " << m_epoch << "\n"
	  << "generation " << m_generation.load() << "\n";
	{
		std::shared_lock lock(m_lock);
		o << "files " << m_files.size() << "\n"
		  << "owned " << m_owned.size() << "\n"
		  << "collisions " << m_collisions.size() << "\n"
		  << "unavailable " << m_unavailable.size() << "\n"
		  << "reparse_skipped " << m_skippedReparse << "\n"
		  << "rescans " << m_rescans << "\n"
		  << "overflows " << m_overflows << "\n";
	}
	{
		std::shared_lock lock(m_lock);
		if (!m_reason.empty())
			o << "reason " << m_reason << "\n";
	}
	// The response tables this process serves to the startup lists (Session 18 correction), as the
	// cache parsed them, and whether the manifest is bound to them -- fixed until a restart.  T rows
	// carry no space, so "key value" readers (the DLL's ParseStatus, the harness) pass over them.
	o << "server_tables " << m_served.size() << "\n"
	  << "server_binding " << (m_manifest.loaded && m_serverBinding.empty() ? 1 : 0) << "\n";
	for (const auto& table : m_served)
		o << "T\t" << table.root << "\t" << table.path << "\t" << table.digest << "\n";
	o << "end\n";
	return o.str();
}

std::string ContentBroker::indexText(uint64_t since, const std::string& epoch) const
{
	const bool full = since == 0 || epoch != m_epoch;
	std::ostringstream o;
	std::shared_lock lock(m_lock);
	o << kProtocol << "\n"
	  << "epoch " << m_epoch << "\n"
	  << "generation " << m_generation.load() << "\n"
	  << "full " << (full ? 1 : 0) << "\n";
	std::vector<const OwnedEntry*> rows;
	for (const auto& [name, e] : m_owned)
		if (full || e.gen > since)
			rows.push_back(&e);
	std::sort(rows.begin(), rows.end(), [](auto a, auto b) { return a->rel < b->rel; });
	for (const auto* e : rows)
		o << "F\t" << e->gen << "\t" << e->size << "\t" << e->basename << "\t" << e->rel << "\t" << e->digest << "\n";
	if (!full)
		for (const auto& [name, gen] : m_tombstones)
			if (gen > since)
				o << "D\t" << gen << "\t" << name << "\n";
	for (const auto& [name, info] : m_unavailable)
		o << "X\t" << info.first << "\t" << name << "\t" << info.second << "\n";
	for (const auto& [name, rels] : m_collisions)
	{
		o << "C\t" << name;
		for (const auto& r : rels)
			o << "\t" << r;
		o << "\n";
	}
	o << "end\n";
	return o.str();
}

ReadResult ContentBroker::read(const std::string& rel, const std::string& expectDigest)
{
	ReadResult r;
	if (!m_enabled || !m_ready)
	{
		r.status = 503;
		r.error = "not-ready";
		return r;
	}
	if (!expectDigest.empty() && !isDigestTag(expectDigest))
	{
		r.status = 400;
		r.error = "bad-digest";
		return r;
	}
	std::string published;   // the identity the index currently offers for this path
	{
		std::shared_lock lock(m_lock);
		const auto it = m_owned.find(lower(basenameOf(rel)));
		// Exact match against the enumerated path only: no normalisation, no
		// basename fallback, so no request string can name another file.
		if (it == m_owned.end() || it->second.rel != rel)
		{
			r.status = 404;
			r.error = "not-owned";
			return r;
		}
		r.gen = it->second.gen;
		published = it->second.digest;
	}
	const auto path = m_root / fromUtf8(rel);

#ifdef _WIN32
	// THE OPEN CAN COLLIDE WITH AN ATOMIC REPLACE OF THIS VERY FILE.  A writer
	// still holding it fails the open with ERROR_SHARING_VIOLATION (or a lock
	// violation); while the old file is being replaced it is DELETE-PENDING,
	// and opening a delete-pending file fails with ERROR_ACCESS_DENIED -- which
	// used to fall through to 404 "missing", telling the client a file was gone
	// that was only changing hands (test_content_broker_wire.py's concurrency
	// case, intermittently; the provider retries a 503 but not a 404).
	// hashFile already treats these as the replace window; so does this now,
	// for about a third of a second.  Not-found is retried briefly too, in case
	// the name itself is in flight, and for the whole window once the file was
	// seen blocked: deleted, its new copy not yet in place.  A file really
	// removed from game_content is still a 404 "missing"; one still blocked
	// after the window is a retryable 503 "busy".
	constexpr int kOpenAttempts = 8;
	HANDLE h = INVALID_HANDLE_VALUE;
	DWORD err = ERROR_SUCCESS;
	bool changingHands = false;
	for (int attempt = 0; attempt < kOpenAttempts; ++attempt)
	{
		// FILE_SHARE_READ only: a writer that still holds the file blocks the
		// open, so a half-written file is never served.
		h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
			FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (h != INVALID_HANDLE_VALUE)
			break;
		err = GetLastError();
		const bool blocked = err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION
			|| err == ERROR_ACCESS_DENIED;
		const bool absent = err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND;
		changingHands = changingHands || blocked;
		if (attempt + 1 == kOpenAttempts || !(blocked || (absent && (attempt < 3 || changingHands))))
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(40));
	}
	if (h == INVALID_HANDLE_VALUE)
	{
		const bool blocked = err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION
			|| err == ERROR_ACCESS_DENIED;
		r.status = blocked ? 503 : 404;
		r.error = blocked ? "busy" : "missing";
		LOG_WARN << "content broker: open " << rel << " failed with Win32 error " << err
			<< (blocked ? " (still blocked after the retry window)" : "");
		return r;
	}
	const auto finalPath = finalPathOf(h);
	if (finalPath.empty() || !startsWithI(finalPath, m_rootFinal))
	{
		CloseHandle(h);
		r.status = 403;
		r.error = "outside-root";
		LOG_WARN << "content broker: refused " << rel << " (final path leaves the content root)";
		return r;
	}
	BY_HANDLE_FILE_INFORMATION before{}, after{};
	GetFileInformationByHandle(h, &before);
	const uint64_t size = (static_cast<uint64_t>(before.nFileSizeHigh) << 32) | before.nFileSizeLow;
	if (size > 256ull * 1024 * 1024)
	{
		CloseHandle(h);
		r.status = 413;
		r.error = "too-large";
		return r;
	}
	r.bytes.resize(static_cast<size_t>(size));
	DWORD got = 0;
	size_t off = 0;
	bool ok = true;
	while (off < r.bytes.size())
	{
		const DWORD chunk = static_cast<DWORD>(std::min<size_t>(r.bytes.size() - off, 1 << 20));
		if (!ReadFile(h, r.bytes.data() + off, chunk, &got, nullptr) || got == 0)
		{
			ok = false;
			break;
		}
		off += got;
	}
	GetFileInformationByHandle(h, &after);
	CloseHandle(h);
	if (!ok || before.nFileSizeLow != after.nFileSizeLow || before.nFileSizeHigh != after.nFileSizeHigh
		|| before.ftLastWriteTime.dwLowDateTime != after.ftLastWriteTime.dwLowDateTime
		|| before.ftLastWriteTime.dwHighDateTime != after.ftLastWriteTime.dwHighDateTime)
	{
		r.bytes.clear();
		r.status = 503;
		r.error = "changed-during-read";
		return r;
	}
#else
	std::error_code ec;
	const auto canon = fs::canonical(path, ec);
	const auto rootStr = m_root.generic_string() + "/";
	if (ec || !canon.generic_string().starts_with(rootStr))
	{
		r.status = 403;
		r.error = "outside-root";
		return r;
	}
	std::ifstream in(canon, std::ios::binary);
	if (!in)
	{
		r.status = 404;
		r.error = "missing";
		return r;
	}
	std::stringstream ss;
	ss << in.rdbuf();
	r.bytes = ss.str();
#endif

	r.digest = sha256Tag(r.bytes);

	if (r.digest != published)
	{
		// These bytes are not the identity the index offers: an edit the
		// watcher has not flushed yet (same size, timestamp restored), or a
		// revert.  Record it BEFORE answering, through the writer path and
		// against the identity still on record, so the generation sent with
		// these bytes is the one every client sees in its next delta.
		uint64_t gen = 0;
		const auto observed = publishObserved(rel, r.digest, gen);
		if (observed != Observed::Published)
		{
			r.bytes.clear();
			r.digest.clear();
			r.status = observed == Observed::NotOwned ? 404 : 503;
			r.error = observed == Observed::NotOwned ? "not-owned" : "changed-during-read";
			return r;
		}
		r.gen = gen;
	}
	if (!expectDigest.empty() && r.digest != expectDigest)
	{
		// The client's scene pinned another version and none is kept: refuse
		// rather than mix versions inside that scene.  digest/gen name the
		// version the file has now (diagnostics only).
		r.bytes.clear();
		r.status = 409;
		r.error = "version-unavailable";
		return r;
	}
	r.status = 200;
	return r;
}

ContentBroker::Observed ContentBroker::publishObserved(const std::string& rel, const std::string& observed, uint64_t& gen)
{
	std::lock_guard writer(m_writer);
	std::string digest;
	const auto current = [&] {
		std::shared_lock lock(m_lock);
		const auto it = m_owned.find(lower(basenameOf(rel)));
		if (it == m_owned.end() || it->second.rel != rel)
			return false;
		digest = it->second.digest;
		gen = it->second.gen;
		return true;
	};
	if (!current())
		return Observed::NotOwned;
	// The watcher or a concurrent reader recorded this version first.
	if (digest == observed)
		return Observed::Published;
	// Nothing was written yet, so applyChanged compares a fresh hash with the
	// identity on record and bumps the generation only for a real change.
	applyChanged({ rel }, "digest changed on read");
	if (!current())
		return Observed::NotOwned;
	return digest == observed ? Observed::Published : Observed::ChangedAgain;
}

std::string ContentBroker::coverageText(const std::string& build, bool detail) const
{
	// Problem names listed per set and class unless detail=1 asks for all.  "absent" (in no
	// source at all) is a problem like the others: absence from every source proves a name
	// cannot be served, not that the startup path does without it (QC 2026-10-20).
	constexpr size_t kListed = 25;
	static const char* const kClasses[] = { "absent", "unowned", "missing", "ambiguous", "unavailable" };
	const std::string clientBuild = build.size() <= 64 && safeProtocolText(build) ? build : std::string{};
	const bool buildMatch = m_manifest.loaded && !clientBuild.empty() && clientBuild == m_manifest.build;

	std::ostringstream o;
	o << kProtocol << "\n"
	  << "coverage 1\n"
	  << "enabled " << (m_enabled ? 1 : 0) << "\n"
	  << "ready " << (m_ready ? 1 : 0) << "\n"
	  << "epoch " << m_epoch << "\n";

	std::shared_lock lock(m_lock);
	const uint64_t generation = m_generation.load();
	o << "generation " << generation << "\n"
	  << "policy " << (m_policyDigest.empty() ? "none" : m_policyDigest) << "\n"
	  << "manifest " << (m_manifest.loaded ? m_manifest.digest : "none") << "\n";
	if (!m_manifest.loaded && !m_manifest.error.empty())
		o << "manifest_error " << oneLine(m_manifest.error) << "\n";
	o << "manifest_build " << (m_manifest.loaded ? m_manifest.build : "none") << "\n"
	  << "client_build " << (clientBuild.empty() ? "none" : clientBuild) << "\n"
	  << "build_match " << (buildMatch ? 1 : 0) << "\n"
	  << "unknown " << m_manifest.unknown.size() << "\n"
	  << "excluded " << m_manifest.excluded.size() << "\n"
	  << "inputs " << m_manifest.clientInputs.size() << "\n";

	std::string reason;
	if (!m_enabled)
		reason = "broker disabled";
	else if (!m_ready)
		reason = "broker not ready";
	else if (!m_manifest.loaded)
		reason = "no startup manifest";
	else if (clientBuild.empty())
		reason = "no client build given";
	else if (!buildMatch)
		reason = "the manifest was derived for client build " + m_manifest.build + ", not " + clientBuild;
	else if (!m_serverBinding.empty())
		reason = m_serverBinding;     // derived from other response rows than this server serves: stale
	else if (!m_manifest.unknown.empty())
		reason = std::to_string(m_manifest.unknown.size()) + " startup list(s) the manifest could not derive";
	else if (m_manifest.clientInputs.empty())
		reason = "the manifest names none of the client's own inputs (inputs.client_files): it cannot be bound to "
			"the tables this client holds";

	std::string rows;   // "P" rows, gathered under the cache lock
	if (m_enabled && m_ready && m_manifest.loaded)
	{
		std::lock_guard cache(m_coverageLock);
		if (!m_coverage.valid || m_coverage.generation != generation)
		{
			// Every name in the tree, owned or not, tells "unowned" from "missing".
			std::unordered_set<std::string> present;
			present.reserve(m_files.size());
			for (const auto& [rel, e] : m_files)
				present.insert(lower(basenameOf(rel)));
			CoverageCache next;
			next.generation = generation;
			for (const auto& [set, names] : m_manifest.sets)
			{
				auto& problems = next.problems[set];
				size_t ok = 0;
				for (const auto& name : names)
				{
					// Root cause first: a collided bundle member is also in its withdrawn bundle.
					if (m_owned.contains(name))
						++ok;
					else if (m_collisions.contains(name))
						problems["ambiguous"].push_back(name);
					else if (m_unavailable.contains(name))
						problems["unavailable"].push_back(name);
					else if (present.contains(name))
						problems["unowned"].push_back(name);
					else if (m_manifest.absent.contains(name))
						problems["absent"].push_back(name);    // exists nowhere: a gap, not an exemption
					else
						problems["missing"].push_back(name);
				}
				next.ok[set] = ok;
			}
			next.valid = true;
			m_coverage = std::move(next);
		}
		std::string summary;
		for (const auto& [set, names] : m_manifest.sets)
		{
			const size_t required = names.size();     // the loader keeps excluded names out of every set
			const auto& problems = m_coverage.problems[set];
			const auto count = [&](const char* cls) {
				const auto it = problems.find(cls);
				return it == problems.end() ? size_t{ 0 } : it->second.size();
			};
			o << "set " << set << " required " << required << " ok " << m_coverage.ok[set];
			std::string missingHere;
			for (const char* cls : kClasses)
			{
				o << " " << cls << " " << count(cls);
				if (count(cls))
					missingHere += (missingHere.empty() ? "" : ", ") + std::to_string(count(cls)) + " " + cls;
			}
			o << "\n";
			if (!missingHere.empty())
				summary += (summary.empty() ? "" : "; ") + ("set " + set + ": " + missingHere);
		}
		if (reason.empty() && !summary.empty())
			reason = summary;
		for (const auto& [set, problems] : m_coverage.problems)
			for (const char* cls : kClasses)
			{
				const auto it = problems.find(cls);
				if (it == problems.end())
					continue;
				const size_t n = detail ? it->second.size() : std::min(it->second.size(), kListed);
				for (size_t i = 0; i < n; ++i)
					rows += "P\t" + set + "\t" + cls + "\t" + it->second[i] + "\n";
			}
	}
	o << "complete " << (reason.empty() ? 1 : 0) << "\n";
	if (!reason.empty())
		o << "reason " << oneLine(reason) << "\n";
	for (const auto& u : m_manifest.unknown)
		o << "U\t" << u << "\n";
	// The client's own files the manifest was derived from: the DLL hashes its copies before it
	// takes the report as proof (the broker cannot see them).
	for (const auto& input : m_manifest.clientInputs)
		o << "I\t" << input.root << "\t" << input.path << "\t" << input.digest << "\n";
	o << rows << "end\n";
	return o.str();
}

} // namespace content
