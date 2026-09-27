#include "App.hpp"
#include "BfWebController.hpp"
#include "WebTerms.hpp"

#include <filesystem>
#include <chrono>
#include <mutex>
#include <unordered_map>

using namespace drogon;
namespace fs = std::filesystem;

namespace
{

/*!
* Locate an MST download part by NAME, wherever it happens to be staged.
*
* THE CLIENT FETCHES THESE FROM A FLAT `/mst/`, NOT `/content/<TABLE>/`.
* That was the single unproven inference in the version/download channel: it
* came from `requestMstFiles` passing the target name as a directory, and the
* `/content` prefix was assumed from how `/sound/` behaves.  It was wrong, and
* it failed in the worst available shape -- `requestMstFiles` runs during BOOT,
* so the client sat on `DLC_PROBLEM_ENCOUNTERED` and the game was unplayable
* rather than merely missing a table.  deploy/log/dlc_404.log had recorded the
* real request all along: `/mst/Ver1_U1XChi09.dat`, five times over.
*
* So this deliberately does NOT care which directory a part is staged in.
* Whatever layout gen_mst_download.py writes, a part reachable by name is
* served -- which is what makes a wrong guess about the layout survivable
* instead of fatal.  mstVersions() still refuses to ANNOUNCE a table whose
* parts are missing, so the two halves stay honest independently.
*
* @param requestPath Request path, untrusted, e.g. "/mst/Ver1_U1XChi09.dat".
* @return Path to the part, or empty when no such part is staged.
*/
fs::path findMstPart(const std::string& requestPath)
{
	const auto slash = requestPath.find_last_of('/');
	const auto name = slash == std::string::npos
		? requestPath : requestPath.substr(slash + 1);

	// Validate against the FORMAT rather than blacklisting traversal: the only
	// legal names are `Ver<n>_<key>.dat` and `Ver<n>_<key>_<part>.dat`, and
	// nothing matching that can hold a separator or "..".
	if (name.size() < 8 || name.compare(0, 3, "Ver") != 0
		|| name.compare(name.size() - 4, 4, ".dat") != 0)
		return {};
	for (const char c : name.substr(0, name.size() - 4))
	{
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
			return {};
	}

	std::error_code ec;
	const fs::path root = fs::path(app().getDocumentRoot()) / "content";
	if (!fs::is_directory(root, ec))
		return {};

	// Flat under the content root first, then each table's own folder.
	std::error_code flatEc;
	if (fs::is_regular_file(root / name, flatEc) && !flatEc)
		return root / name;

	std::error_code iterEc;
	auto it = fs::directory_iterator(root, iterEc);
	if (iterEc)
		return {};
	for (const auto& entry : it)
	{
		std::error_code dirEc;
		if (!entry.is_directory(dirEc) || dirEc)
			continue;
		std::error_code partEc;
		const auto candidate = entry.path() / name;
		if (fs::is_regular_file(candidate, partEc) && !partEc)
			return candidate;
	}
	return {};
}

/*!
* Find any content file by NAME, wherever the package happens to put it.
*
* ⚠ WHERE A FILE IS PACKAGED AND WHERE THE CLIENT ASKS FOR IT ARE TWO DIFFERENT
* FACTS, and only one of them is ours.  The Town map ships as
* `content/sam/MapVillage/MapVillage_640x384.png`; the client asks for it under
* `_dlcbundle/MapVillage/`.  A miss there is not a blank tile -- a scene that
* cannot load its art null-derefs, which is why Town, the shop, the merit
* exchange and sending a friend a gift were all crashing on the release build.
*
* The obvious fix is to move the files, but that means re-cutting the download
* mirror every time the client surprises us with another path, and it only ever
* fixes the paths we already know about.  Resolving by name fixes the whole
* class at once and keeps the package layout free to change: the server looks
* for the basename anywhere under the content root and serves whatever it
* finds.  Same reasoning as findMstPart above, which exists because guessing
* the MST layout was wrong once and stalled boot.
*
* The lookup is served from an index rather than a walk -- the tree is 76,000
* files, so scanning per request would turn a cold cache into a stall -- but a
* MISS rescans (at most once every 10s) so files dropped in after boot are
* found without a restart.  See the note at the miss below.
*
* @param requestPath Request path, untrusted, e.g. "/content/_dlcbundle/MapVillage/x.png".
* @return Path to a file with that name, or empty when nothing matches.
*/
fs::path findContentByName(const std::string& requestPath)
{
	const auto slash = requestPath.find_last_of('/');
	const auto name = slash == std::string::npos
		? requestPath : requestPath.substr(slash + 1);

	// Only ever resolve a plain filename.  Anything carrying a separator or
	// ".." is a traversal attempt, not a packaging mismatch.
	if (name.empty() || name == "." || name.find("..") != std::string::npos)
		return {};

	static std::mutex guard;
	static std::unordered_map<std::string, fs::path> index;
	static std::chrono::steady_clock::time_point builtAt{};

	const auto rebuild = [] {
		const fs::path root = fs::path(app().getDocumentRoot()) / "content";
		std::error_code ec;
		index.clear();
		if (!fs::is_directory(root, ec))
			return;
		fs::recursive_directory_iterator it(
			root, fs::directory_options::skip_permission_denied, ec);
		if (ec)
			return;
		size_t files = 0;
		for (const auto& entry : it)
		{
			std::error_code fileEc;
			if (!entry.is_regular_file(fileEc) || fileEc)
				continue;
			++files;
			// FIRST WINS, and the walk is depth-first from the root, so a file
			// sitting at a shallower path beats a copy buried deeper.  When the
			// same name exists twice the shallower one is the canonical drop,
			// and either copy renders the same art anyway.
			index.emplace(entry.path().filename().string(), entry.path());
		}
		builtAt = std::chrono::steady_clock::now();
		LOG_INFO << "content index: " << index.size() << " distinct filename(s) from "
		         << files << " file(s) -- used to answer requests whose path does "
		            "not match how the package is laid out";
	};

	std::lock_guard<std::mutex> lock(guard);
	if (builtAt == std::chrono::steady_clock::time_point{})
		rebuild();

	auto hit = index.find(name);
	if (hit == index.end())
	{
		// ⚠ A MISS MUST BE ABLE TO SEE FILES ADDED SINCE BOOT.  An index built
		// once per process means anything dropped into the content tree after
		// startup stays invisible until a restart -- and dropping files in is
		// exactly how this gap gets closed (tools/seed_client_assets.py, or a
		// hand-copied folder).  The client asking to download is the moment to
		// look again, so a miss rescans.
		//
		// Rate-limited because a genuinely absent file is requested repeatedly:
		// the client retries, and rescanning 76,000 files per retry would turn
		// one missing asset into a stall.
		const auto now = std::chrono::steady_clock::now();
		if (now - builtAt > std::chrono::seconds(10))
		{
			rebuild();
			hit = index.find(name);
		}
	}
	return hit == index.end() ? fs::path{} : hit->second;
}

} // namespace

// This code was intended for generating placeholders in assets
//#define ENABLE_FILE_GENERATOR 1

#if ENABLE_FILE_GENERATOR
// Parse dimensions from filename (e.g., "MapVillage_640x0809.png" -> 640, 809)
static bool parseDimensions(const std::string& filename, int& width, int& height)
{
    size_t lastUnderscore = filename.find_last_of('_');
    size_t dotPos = filename.find_last_of('.');
    if (lastUnderscore == std::string::npos || dotPos == std::string::npos || lastUnderscore >= dotPos) {
        return false;
    }

    std::string dimStr = filename.substr(lastUnderscore + 1, dotPos - lastUnderscore - 1);
    size_t xPos = dimStr.find('x');
    if (xPos == std::string::npos) return false;

    try {
        width = std::stoi(dimStr.substr(0, xPos));
        height = std::stoi(dimStr.substr(xPos + 1));
        return true;
    }
    catch (...) {
        return false;
    }
}

// Generate a minimal PNG with specified dimensions
static std::vector<char> generatePlaceholderPng(int width, int height) {
    std::vector<char> pngData;

    // PNG signature
    const char signature[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    pngData.insert(pngData.end(), signature, signature + 8);

    // IHDR chunk
    uint32_t ihdrLength = htonl(13);
    pngData.insert(pngData.end(), (char*)&ihdrLength, (char*)&ihdrLength + 4);
    const char ihdrType[] = "IHDR";
    pngData.insert(pngData.end(), ihdrType, ihdrType + 4);

    uint32_t w = htonl(width);
    uint32_t h = htonl(height);
    pngData.insert(pngData.end(), (char*)&w, (char*)&w + 4);
    pngData.insert(pngData.end(), (char*)&h, (char*)&h + 4);

    const char ihdrData[] = { 8, 2, 0, 0, 0 }; // Bit depth 8, RGB, no compression, filter, interlace
    pngData.insert(pngData.end(), ihdrData, ihdrData + 5);

    uint32_t ihdrCrc = htonl(0); // Simplified CRC
    pngData.insert(pngData.end(), (char*)&ihdrCrc, (char*)&ihdrCrc + 4);

    // IDAT chunk (minimal data for a blank image)
    uint32_t idatLength = htonl(12 + width * 4 * height); // Adjust length based on dimensions
    pngData.insert(pngData.end(), (char*)&idatLength, (char*)&idatLength + 4);
    const char idatType[] = "IDAT";
    pngData.insert(pngData.end(), idatType, idatType + 4);

    // Minimal data: transparent pixels (RGBA)
    std::vector<char> idatData;
    for (int y = 0; y < height; ++y) {
        idatData.push_back(0); // Filter type
        for (int x = 0; x < width; ++x) {
            idatData.push_back(0); idatData.push_back(0); idatData.push_back(0); idatData.push_back(0); // Transparent
        }
    }
    pngData.insert(pngData.end(), idatData.begin(), idatData.end());

    uint32_t idatCrc = htonl(0);
    pngData.insert(pngData.end(), (char*)&idatCrc, (char*)&idatCrc + 4);

    // IEND chunk
    uint32_t iendLength = htonl(0);
    pngData.insert(pngData.end(), (char*)&iendLength, (char*)&iendLength + 4);
    const char iendType[] = "IEND";
    pngData.insert(pngData.end(), iendType, iendType + 4);
    uint32_t iendCrc = htonl(0);
    pngData.insert(pngData.end(), (char*)&iendCrc, (char*)&iendCrc + 4);

    return pngData;
}
#endif

void BfWebController::HandleWebPage(const HttpRequestPtr& rq, std::function<void(const HttpResponsePtr&)>&& callback)
{
    DumpLog log;
    theServer()->tryOpenHttpDumpLog("webpage", log);
    log << rq;

	if (rq->getPath() == "/bf/web/terms.htm")
	{
        // TODO: convert this to a drogon ctl controller
        auto resp = HttpResponse::newFileResponse((const unsigned char*)kWebTermsData, strlen(kWebTermsData), "", CT_TEXT_HTML);
        resp->setCloseConnection(true); // kill session
		callback(resp);
		return;
	}

    // Construct the full filesystem path
	auto path = getDocumentRoot() + rq->getPath();
    LOG_DEBUG << "Requesting path: " << path;

    // Serve existing file if it exists
	if (!fs::exists(path) || fs::is_directory(path))
	{
		// Before giving up: an MST download part is staged under its TABLE's
		// folder, but the client asks for it from a flat /mst/.  This is the
		// only 404 on this server that wedges BOOT, so it gets a second look.
		if (const auto part = findMstPart(rq->getPath()); !part.empty())
		{
			LOG_INFO << "MST download: " << rq->getPath() << " served from " << part.string();
			auto resp = HttpResponse::newFileResponse(part.string());
			resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);
			callback(resp);
			return;
		}

		// Still nothing at the path the client asked for.  Before 404ing --
		// which for an asset is a crash, not a gap -- look the file up by name
		// anywhere under the content root.  See findContentByName.
		if (const auto asset = findContentByName(rq->getPath()); !asset.empty())
		{
			LOG_INFO << "content: " << rq->getPath() << " served from "
			         << asset.string() << " (packaged elsewhere)";
			// Logged as well as served: the line is the evidence for where the
			// package SHOULD put it, and without it a silently-healed mismatch
			// is invisible until someone re-cuts the mirror and it breaks again.
			// ⚠ "\n" not '\n' -- DumpLog has no char overload and writes a lone
			// char as its integer value, which puts a stray "10" in the log.
			logDlc() << rq->getPath() << "  -> resolved by name: " << asset.string() << "\n";
			callback(HttpResponse::newFileResponse(asset.string()));
			return;
		}

		// DumpLog does not terminate a statement, so every miss used to run into
		// the next one and the log could not be read back as a list of paths.
		logDlc() << rq->getPath() << "\n";
		callback(HttpResponse::newNotFoundResponse());
	}
	else
	{
#if !ENABLE_FILE_GENERATOR
		callback(HttpResponse::newFileResponse(path));
#else
        LOG_INFO << "Generating placeholder for: " << path;

        try {
            // Ensure directory exists
            fs::path dirPath = path.substr(0, path.find_last_of('/'));
            if (!dirPath.empty() && !fs::exists(dirPath)) {
                fs::create_directories(dirPath);
            }

            std::ofstream outFile(path, std::ios::binary);
            if (!outFile.is_open()) {
                LOG_ERROR << "Failed to create placeholder: " << path;
                callback(HttpResponse::newNotFoundResponse());
                return;
            }

            // Generate placeholder based on file extension
            std::string filename = rq->getPath().substr(rq->getPath().find_last_of('/') + 1);
            if (filename.find(".png") != std::string::npos) {
                int width = 1, height = 1;
                parseDimensions(filename, width, height);
                auto placeholderPng = generatePlaceholderPng(width, height);
                outFile.write(placeholderPng.data(), placeholderPng.size());
            }
            else if (filename.find(".csv") != std::string::npos) {
                std::string placeholderCsv = "id,value\n0,placeholder";
                outFile.write(placeholderCsv.data(), placeholderCsv.size());
            }
            else if (filename.find(".dat") != std::string::npos) {
                std::string placeholderDat = "placeholder_game_data";
                outFile.write(placeholderDat.data(), placeholderDat.size());
            }
            else {
                std::string placeholderGeneric = "placeholder";
                outFile.write(placeholderGeneric.data(), placeholderGeneric.size());
            }
            outFile.close();
            LOG_INFO << "Generated placeholder for: " << path;

            // Serve the newly created placeholder file
            auto resp = HttpResponse::newFileResponse(path);
            // Drogon will automatically detect MIME type from extension
            callback(resp);
        }
        catch (const std::exception& e) {
            LOG_ERROR << "Error generating placeholder: " << e.what();
            callback(HttpResponse::newNotFoundResponse());
        }
#endif
	}
}

void BfWebController::HandleDefault(const HttpRequestPtr& rq, std::function<void(const HttpResponsePtr&)>&& callback)
{
	auto resp = HttpResponse::newHttpResponse();
	resp->setStatusCode(k200OK);
	resp->setBody("gimuserver server works!");
	callback(resp);
}
