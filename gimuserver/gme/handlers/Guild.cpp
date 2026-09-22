#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/Guilds.hpp>

// The guild create-and-invite slice.
//
// Four handlers, all of whose wire shapes are Binary-confirmed in
// net/guild.kdl -- the requests from their producing createBody, the responses
// from the class GameResponseParser::getResponseObject dispatches the key to.
//
// WHY THESE FOUR.  They are the whole loop a player can complete here: look at
// the guild screen (which quotes the founding fee when you have none), found
// one, see who you could bring in, bring them in.  The other forty guild
// requests are raids, boards, rankings and research, none of which mean
// anything until a guild exists.

// GuildInfo (138ba8d4) and GuildJoinedList (3890ab5j) -- both identity-only
// requests that answer "what guild am I in?".  They share this handler because
// they share the question; the reply is the same singleton either way.
//
// A player with no guild gets the FOUNDING FEE and no guild block at all --
// the field is optional precisely so it can be absent, because GuildInfoResponse
// is a singleton with no clear and a zeroed row would read as "guild 0".
HANDLEF(GuildInfo)
{
	(void)session;
	::GuildInfoReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "GuildInfo: parse error: " << glz::format_error(ec, json);
	}
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	const auto guild = co_await gme::loadGuild(theDb(), identity);
	if (!guild)
	{
		// No guild: send the FOUNDING FEE instead.  Nothing else can -- there is
		// no GuildCreateCostRequest in the binary, so this reply is the only
		// place the cost reaches the player.
		::GuildInfoResp cost{};
		::GuildCreationInfo info{};
		info.currency_type = gme::kGuildCreateCurrency;
		info.required_amount = static_cast<int32_t>(gme::kGuildCreateCost);
		cost.creation_info.push_back(std::move(info));

		LOG_INFO << "GuildInfo: " << identity.userId
			<< " is not in a guild; sent the founding fee";
		co_return HandleResult::success(glz::write_json(cost).value_or("{}"));
	}

	const auto handle = (co_await db::DatabaseInterface::read(
		theDb(), "user_info",
		{ db::Data("username"), db::Lookup("id", identity.userId) }))
		.front<std::string>("username");

	::GuildInfoResp resp{};
	resp.guild.push_back(gme::guildInfoBlock(*guild, handle));
	// The Hall's roster rides along.  Without it the member list is empty and
	// tapping a member is a null dereference -- an access violation with no
	// dialog, which no empty-body stub can prevent.
	resp.members = co_await gme::guildRoster(theDb(), identity);

	LOG_INFO << "GuildInfo: " << identity.userId << " is in \"" << guild->name
		<< "\" (" << guild->members_count << " member(s))";
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

// GuildCreate (g298Da10) -- found one.
//
// The fee is charged BEFORE the guild is written and in the same statement that
// checks it, so a player who cannot afford it cannot create one by racing two
// requests: the UPDATE only matches while the balance still covers it.
HANDLEF(GuildCreate)
{
	(void)session;
	::GuildCreateReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "GuildCreate: parse error: " << glz::format_error(ec, json);
	}
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// req.guild is the node itself, not a list: `[T]::size(1)` generates an
	// embedded object that serialises as a one-element array.  It and `[T]` are
	// not interchangeable, and the difference is invisible on the wire.
	const auto& node = req.guild;

	if (node.name.empty())
	{
		LOG_WARN << "GuildCreate: " << identity.userId << " sent an empty guild name";
		co_return HandleResult::error("Bad request", "A guild needs a name");
	}

	// guild_art_id arrives QUOTED even though GuildInfo reads it back as an int.
	int32_t artId = gme::kDefaultGuildArtId;
	try { artId = std::stoi(node.guild_art_id); }
	catch (const std::exception&)
	{
		LOG_WARN << "GuildCreate: unparseable guild_art_id "
			<< node.guild_art_id << "; defaulting to " << artId;
	}

	const auto charged = co_await theDb()->execSqlCoro(
		"UPDATE user_info SET zel = zel - $1 WHERE id = $2 AND zel >= $1;",
		static_cast<int64_t>(gme::kGuildCreateCost), identity.userId);
	if (charged.affectedRows() == 0)
	{
		LOG_INFO << "GuildCreate: " << identity.userId << " cannot afford the "
			<< gme::kGuildCreateCost << " zel fee";
		co_return HandleResult::error("Insufficient funds", "Not enough Zel to found a guild");
	}

	const auto guild = co_await gme::createGuild(
		theDb(), identity, node.name, node.description, artId);
	if (!guild)
	{
		// Refund: the charge landed but the guild did not, so the player must
		// not be left paying for nothing.
		co_await theDb()->execSqlCoro(
			"UPDATE user_info SET zel = zel + $1 WHERE id = $2;",
			static_cast<int64_t>(gme::kGuildCreateCost), identity.userId);
		LOG_WARN << "GuildCreate: " << identity.userId << " could not create a guild; fee refunded";
		co_return HandleResult::error("Already in a guild", "This Summoner already belongs to a guild");
	}

	const auto handle = (co_await db::DatabaseInterface::read(
		theDb(), "user_info",
		{ db::Data("username"), db::Lookup("id", identity.userId) }))
		.front<std::string>("username");

	::GuildInfoResp resp{};
	resp.guild.push_back(gme::guildInfoBlock(*guild, handle));
	resp.members = co_await gme::guildRoster(theDb(), identity);
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

// GuildRecomendedMember (ja5Enusw) -- who the player could invite.
//
// ⚠ A FULL REPLACE: readParam @0x1C64A38 calls removeAllObjects, so this must
// always carry the COMPLETE candidate list.
//
// The candidates are the player's FRIENDS, which is recovered rather than
// chosen: GuildRecomendedMemberInfo's own setters are setFriendUserUnitID,
// setFriendUserUnitLevel and setFriendUserImageType.
HANDLEF(GuildRecomendedMember)
{
	(void)session;
	::GuildInfoReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "GuildRecomendedMember: parse error: " << glz::format_error(ec, json);
	}
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	::GuildRecomendedMemberResp resp{};
	resp.members = co_await gme::invitableFriends(theDb(), identity);

	LOG_INFO << "GuildRecomendedMember: " << resp.members.size()
		<< " invitable friend(s) for " << identity.userId;
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

// GuildMemberUpdate (ad81b8at) -- bring friends in.
//
// AUTHORED: they always accept.  A real invite was a request the other player
// answered; the Summoners here are simulated, so there is nobody to ask, and a
// chance of refusal would be a dice roll wearing a social system's clothes.
//
// The reply is the refreshed guild, because the member count on the screen the
// player is looking at has just changed and GuildInfoResponse is the only thing
// that updates it.
HANDLEF(GuildMemberUpdate)
{
	(void)session;
	::GuildMemberUpdateReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "GuildMemberUpdate: parse error: " << glz::format_error(ec, json);
	}
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	std::vector<std::string> ids;
	for (const auto& node : req.members)
	{
		if (!node.user_id.empty())
			ids.push_back(node.user_id);
	}

	const auto joined = co_await gme::inviteFriends(theDb(), identity, ids);
	LOG_INFO << "GuildMemberUpdate: " << joined << " of " << ids.size()
		<< " invite(s) accepted for " << identity.userId;

	const auto guild = co_await gme::loadGuild(theDb(), identity);
	if (!guild)
		co_return HandleResult::success("{}");

	const auto handle = (co_await db::DatabaseInterface::read(
		theDb(), "user_info",
		{ db::Data("username"), db::Lookup("id", identity.userId) }))
		.front<std::string>("username");

	::GuildInfoResp resp{};
	resp.guild.push_back(gme::guildInfoBlock(*guild, handle));
	resp.members = co_await gme::guildRoster(theDb(), identity);
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

// InboxMessageManage (rYSfaC4P) -- the message box.
//
// Not a guild request by name, which is why the first guild sweep missed it and
// why "View messages" answered Unsupported request: rYSfaC4P.  It belongs with
// the guild code all the same: MessageInboxResponse carries a whole guild per
// message plus setGuildInviteID, and that invite id is exactly what
// GuildInviteManage sends back.  The inbox is the receiving half of an invite.
//
// EMPTY FOR NOW, honestly rather than silently: nothing on this server sends the
// player a message.  The friends are simulated and always accept, so no invite
// ever comes the other way, and there is no mail. An empty list is the true
// answer, and it is safe -- D9dkXSPY is a full replace but a zero-row array
// never reaches readParam, so it cannot clear anything either.
HANDLEF(InboxMessageManage)
{
	(void)session;
	::InboxMessageManageReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "InboxMessageManage: parse error: " << glz::format_error(ec, json);
	}
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	if (!req.nodes.empty())
	{
		// Nothing to act on, but say which messages were named so a capture shows
		// it rather than the request vanishing.
		LOG_INFO << "InboxMessageManage: " << req.nodes.size()
			<< " message action(s) from " << identity.userId << "; no inbox to apply them to";
	}

	::MessageInboxResp resp{};
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

// GuildRanking (2b9D01b4) and GuildRankingDetail (7ekSBz2y).
//
// BOTH CRASHED AS EMPTY-BODY STUBS, and for the same reason the Guild Hall did:
// the stub keeps the session alive where an unregistered GroupId would close it,
// but a scene that needs rows still dereferences the rows it did not get.  Both
// responses are REPLACE lists (removeAllObjects in their readParam), so a reply
// is always the whole board.
//
// WHAT IS RANKED.  Every guild this server knows, ordered by prestige points.
// That is one guild today, which is an honest leaderboard for a single-player
// offline server rather than a padded one -- inventing rival guilds would put
// names on the board that exist nowhere else in the save.
HANDLEF(GuildRanking)
{
	(void)session;
	::GuildRankingReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "GuildRanking: parse error: " << glz::format_error(ec, json);
	}
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// Echo the tab and season the screen asked for rather than assuming 0/1 --
	// createBody @0x1C63E2C sends both, and the season it sends comes from
	// GuildRaidRoundInfo::getSeasonID(), i.e. from what we told it last time.
	const int32_t askedType = req.ranking.empty() ? 0 : req.ranking.front().ranking_type;
	const int32_t askedSeason = req.ranking.empty() || req.ranking.front().season_id <= 0
		? gme::kGuildRaidSeason : req.ranking.front().season_id;

	const auto rows = co_await theDb()->execSqlCoro(
		"SELECT guild_id, name, description, guild_art_id, experience, prestige_point"
		" FROM user_guilds ORDER BY prestige_point DESC, guild_id ASC;");

	::GuildRankingResp resp{};
	int32_t place = 0;
	for (const auto& r : rows)
	{
		::GuildRankingInfo row{};
		row.ranking          = ++place;
		row.ranking_type     = askedType;
		row.guild_id         = r["guild_id"].as<int32_t>();
		row.name             = r["name"].as<std::string>();
		row.description      = r["description"].as<std::string>();
		row.guild_art_id     = r["guild_art_id"].as<int32_t>();
		row.guild_level      = gme::guildLevelFor(r["experience"].as<int32_t>());
		row.prestige_points  = r["prestige_point"].as<int32_t>();
		row.season_id        = askedSeason;
		resp.ranking.push_back(std::move(row));
	}

	// THE SEASON LIST IS LOAD-BEARING.  GuildRaidRankingResultScene indexes
	// GuildRaidSeasonDataInfoList with no null check, so an empty list is the
	// crash at PE +0x2FEEB1 (`mov eax,[eax+0x14]` on a nulled eax).  One row is
	// enough and one row is honest: this server has run exactly one season.
	for (int32_t season = 1; season <= gme::kGuildRaidSeason; ++season)
	{
		::GuildRaidSeasonDataInfo row{};
		row.season_id = season;
		resp.seasons.push_back(std::move(row));
	}
	resp.round.push_back(gme::guildRaidRound());

	LOG_INFO << "GuildRanking: " << resp.ranking.size()
		<< " guild(s) on the board for " << identity.userId
		<< ", type " << askedType << ", season " << askedSeason
		<< ", " << resp.seasons.size() << " season(s) selectable";
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

// The per-member breakdown behind a board row.
//
// Built from the SAME roster the Guild Hall draws, so a member cannot appear on
// one and not the other.  Contribution is not tracked anywhere yet, so every
// member scores 0 -- which is true, rather than a fabricated ladder.  When a
// contribution source exists it goes in gE2NN2xi and the ordering follows.
HANDLEF(GuildRankingDetail)
{
	(void)session;
	::GuildInfoReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "GuildRankingDetail: parse error: " << glz::format_error(ec, json);
	}
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	::GuildRankingDetailResp resp{};
	const auto guild = co_await gme::loadGuild(theDb(), identity);
	if (!guild)
	{
		LOG_INFO << "GuildRankingDetail: " << identity.userId
			<< " is in no guild; sending an empty board";
		co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
	}

	const auto roster = co_await gme::guildRoster(theDb(), identity);
	int32_t place = 0;
	for (const auto& member : roster)
	{
		::GuildRankingDetailInfo row{};
		row.ranking       = ++place;
		row.ranking_type  = 0;
		row.guild_id      = guild->guild_id;
		row.guild_name    = guild->name;
		row.guild_level   = gme::guildLevelFor(guild->experience);
		row.member_name   = member.handle_name;
		row.user_id       = member.user_id;
		row.bcp_points    = 0;
		row.unit_id       = member.unit_id;
		row.unit_lvl      = member.unit_lv;
		row.unit_img_type = member.unit_img_type;
		resp.detail.push_back(std::move(row));
	}

	LOG_INFO << "GuildRankingDetail: " << resp.detail.size()
		<< " member(s) of guild " << guild->guild_id;
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

// Everything else in the guild set.
//
// Answers an empty body so the session survives.  An unregistered GroupId and a
// handler error both produce GmeErrorCommand::Close, which reaches the player as
// a crash with nothing in deploy/log to explain it; an empty reply gets them a
// blank panel they can back out of instead.
//
// ⚠ NOT a claim that these are done.  Each is a feature still to be written, and
// the GroupId is logged so a capture shows which screen asked for what.
HANDLEF(GuildUnimplemented)
{
	(void)session;
	LOG_INFO << "Guild: unimplemented guild request, answering empty: " << json;
	co_return HandleResult::success("{}");
}
