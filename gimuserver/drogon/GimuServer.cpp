#include "App.hpp"
#include "GimuServer.hpp"

#include <gimuserver/archive/GachaArchiver.hpp>
#include <gimuserver/archive/MissionArchiver.hpp>
#include <gimuserver/archive/UnitArchiver.hpp>
#include <gimuserver/gme/common/BraveSlots.hpp>
#include <gimuserver/gme/common/DailySpin.hpp>
#include <gimuserver/gme/common/MysteryChest.hpp>

GimuServer::GimuServer() : m_dlc_error_log(), m_have_log(false), m_cache() {}

void GimuServer::initAndStart(const Json::Value& config)
{
	const auto& extralog = config["extra_log"];
	if (extralog["enable"].asBool())
	{
		std::string dir = extralog["directory"].asCString();
		std::string log_prefix = extralog["http_log_prefix"].asString();

		std::filesystem::path logPrefixPath(dir);
		logPrefixPath /= log_prefix;
		m_http_log_prefix = logPrefixPath.string();

		m_http_log_suffix = extralog["http_log_suffix"].asString();
		std::string dlcLogName = extralog["dlc_error_log_name"].asString();

		if (!std::filesystem::is_directory(dir))
		{
			if (!std::filesystem::create_directory(dir))
			{
				LOG_WARN << "Cannot create log directory!";
				m_have_log = false;
			}
			else
			{
				m_have_log = true;
			}
		}
		else
		{
			m_have_log = true;
		}

		if (m_have_log)
		{
			std::filesystem::path dlcPath(dir);
			dlcPath /= dlcLogName;
			m_dlc_error_log.open(dlcPath.string());
		}
	}

	const auto& server = config["server"];
	m_cache.Setup(server);
	UnitArchiver::instance().setup(server);
	MissionArchiver::instance().setup(server);
	GachaArchiver::instance().setup(server);
	// Daily Spin reward table.  Authored data rather than MST, so it lives in
	// the archive alongside the other curated tables (handbook §6.15 rule 3).
	gme::loadDailySpinArchive(server["archive_root"].asString());
	gme::loadMysteryChestArchive(server["archive_root"].asString());
	gme::loadBraveSlotArchive(server["archive_root"].asString());
}

void GimuServer::shutdown() {}
