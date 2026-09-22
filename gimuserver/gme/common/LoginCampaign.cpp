#include "App.hpp"

#include <gimuserver/gme/common/LoginCampaign.hpp>
#include <gimuserver/gme/common/Gifts.hpp>   // giftToday(), the shared day key
#include <gimuserver/utils/JsonFile.hpp>

#include <algorithm>

namespace gme
{

namespace
{

std::vector<LoginCampaignDayRow> g_days;

} // namespace

void loadLoginCampaignArchive(const std::string& archiveRoot)
{
	if (archiveRoot.empty())
	{
		LOG_WARN << "LoginCampaign: archive_root is empty, calendar not loaded";
		return;
	}

	try
	{
		g_days = LoadJson<std::vector<LoginCampaignDayRow>>(archiveRoot, "login_campaign.json");
	}
	catch (const std::exception& e)
	{
		// ⚠ LoadJson is STRICT about unknown keys -- an annotation key in the
		// file does not document it, it stops it loading, and the failure shows
		// up as a calendar that silently pays nothing.
		LOG_ERROR << "LoginCampaign: failed to load login_campaign.json: " << e.what();
		return;
	}

	std::sort(g_days.begin(), g_days.end(),
		[](const LoginCampaignDayRow& a, const LoginCampaignDayRow& b) { return a.day < b.day; });

	LOG_INFO << "LoginCampaign: " << g_days.size() << " calendar day(s) loaded";
}

const std::vector<LoginCampaignDayRow>& loginCampaignDays()
{
	return g_days;
}

namespace
{

/*!
* Pay one day's prizes.
*
* Everything goes through the PRESENT BOX, which is the whole reason this works
* without a claim request: the calendar's own claim is animation only, so the
* prize has to be somewhere the player can actually collect it afterwards.
*
* Honor is the exception.  The present box cannot carry it -- Present.cpp's
* dispatch has no case for friend points, and the client's own present
* vocabulary has no Honor entry to name one -- so it is credited straight to
* the balance the way MissionEnd credits helper Honor.  Inventing a present
* type for it would put a tile in the box that the client cannot draw.
*/
drogon::Task<void> grantLoginCampaignDay(
	const db::Database database,
	const UserIdentity identity,
	const LoginCampaignDayRow& row)
{
	for (const auto& prize : row.rewards)
	{
		if (prize.count <= 0)
			continue;

		if (prize.kind == "honor")
		{
			co_await database->execSqlCoro(
				"UPDATE user_info SET friend_points = friend_points + $1 WHERE id = $2;",
				prize.count, identity.userId);
			continue;
		}

		// present_type, per Present.cpp's dispatch.  An unknown kind is skipped
		// loudly rather than banked as a tile the box cannot pay out.
		int32_t presentType = 0;
		std::string targetId;
		if      (prize.kind == "gem")      presentType = 8;
		else if (prize.kind == "zel")      presentType = 3;
		else if (prize.kind == "karma")    presentType = 11;
		else if (prize.kind == "unit")   { presentType = 6; targetId = std::to_string(prize.id); }
		else if (prize.kind == "item")   { presentType = 4; targetId = std::to_string(prize.id); }
		else if (prize.kind == "ticket")   presentType = 8000;
		else if (prize.kind == "selector")
		{
			presentType = 8005;
			// The 8005 target is the SELECTOR id, not a unit -- it is what the
			// present box resolves the ticket's name through.
			targetId = std::to_string(prize.id);
		}
		else
		{
			LOG_WARN << "LoginCampaign: day " << row.day << " has unknown kind '"
				<< prize.kind << "'; skipped";
			continue;
		}

		// A unit present carries ONE unit and repeats, because the present box
		// pays target_cnt copies of a currency but a single unit per tile.
		if (presentType == 6)
		{
			for (int32_t i = 0; i < prize.count; ++i)
				co_await addUserPresent(database, identity, presentType, targetId, 1, 0,
					"Login Bonus Day " + std::to_string(row.day));
		}
		else
		{
			const std::string note = presentType == 8005
				? std::string("This ticket can be used to select from a weekly refreshing"
					" (Friday at 0:00 Server Time) list of 10 heroes")
				: "Login Bonus Day " + std::to_string(row.day);
			co_await addUserPresent(database, identity, presentType, targetId, prize.count, 0, note);
		}
	}

	LOG_INFO << "LoginCampaign: granted day " << row.day << " to " << identity.userId
		<< " (" << row.rewards.size() << " prize(s))";
}

} // namespace

drogon::Task<LoginCampaignState> advanceLoginCampaign(
	const db::Database database,
	const UserIdentity identity)
{
	LoginCampaignState state{};
	state.total_days = static_cast<int32_t>(g_days.size());

	if (g_days.empty())
		co_return state;

	const auto today = giftToday();

	const auto rows = co_await database->execSqlCoro(
		"SELECT current_day, last_day FROM user_login_campaign WHERE user_id = $1;",
		identity.userId);

	int32_t currentDay = 0;
	std::string lastDay;
	if (!rows.empty())
	{
		currentDay = rows[0]["current_day"].as<int32_t>();
		lastDay = rows[0]["last_day"].as<std::string>();
	}

	const auto finalCell = g_days.back().day;

	if (lastDay == today)
	{
		// Already counted today: same cell, nothing granted, and the calendar does
		// NOT rise again.
		//
		// ⚠ TRUE EXACTLY ONCE PER DAY, on the call that advances the day.  This
		// was briefly changed to stay true for the whole day, on the theory that a
		// background poll could spend the flag before the player reached Home.
		// That was over-correcting a symptom I had caused myself: the reason the
		// grid did not appear on 2026-09-17 was that WIRE TESTS had already
		// consumed the day, not anything the client did.  In ordinary play the
		// advancing call is UserInfo at login, HomeScene2::updateEvent reads the
		// singleton it populated, and the calendar rises once.  Leaving it true all
		// day made it rise on every single return to Home.
		//
		// The cost of this choice is that a wire test against a live save still
		// eats the day.  That is the right trade -- a tool is allowed to disturb
		// the thing it is testing; the player is not.
		state.current_day = currentDay;
		state.first_for_the_day = false;
		co_return state;
	}

	if (currentDay >= finalCell)
	{
		// The run is over.  Keep the grid lit, stop paying, and still record the
		// day so this branch is reached once rather than on every request.
		co_await database->execSqlCoro(
			"INSERT INTO user_login_campaign (user_id, current_day, last_day) VALUES ($1, $2, $3)"
			" ON CONFLICT(user_id) DO UPDATE SET last_day = excluded.last_day;",
			identity.userId, currentDay, today);
		state.current_day = currentDay;
		state.first_for_the_day = false;
		co_return state;
	}

	const auto nextDay = currentDay + 1;

	// Write BEFORE granting.  If a grant throws halfway the player keeps what
	// landed and does not get the same day again on the next request; the other
	// order would re-run the whole day and could pay the early prizes twice.
	co_await database->execSqlCoro(
		"INSERT INTO user_login_campaign (user_id, current_day, last_day) VALUES ($1, $2, $3)"
		" ON CONFLICT(user_id) DO UPDATE SET current_day = excluded.current_day,"
		" last_day = excluded.last_day;",
		identity.userId, nextDay, today);

	const auto cell = std::find_if(g_days.begin(), g_days.end(),
		[nextDay](const LoginCampaignDayRow& d) { return d.day == nextDay; });
	if (cell != g_days.end())
		co_await grantLoginCampaignDay(database, identity, *cell);
	else
		LOG_WARN << "LoginCampaign: no authored cell for day " << nextDay;

	state.current_day = nextDay;
	state.first_for_the_day = true;
	co_return state;
}

::UserLoginCampaignInfo loginCampaignInfo(const LoginCampaignState& state)
{
	::UserLoginCampaignInfo info{};
	info.id = kLoginCampaignId;
	info.current_day = state.current_day;
	info.total_days = state.total_days;
	info.first_for_the_day = state.first_for_the_day;
	return info;
}

} // namespace gme
