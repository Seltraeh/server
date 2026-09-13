#pragma once

#include <gimuserver/archive/GachaArchiver.hpp>
#include <gimuserver/gme/common/Common.hpp>

#include <string>
#include <vector>

// Selector summon banners — the rail tiles that appear only while the player
// holds the matching ticket.
//
// The summon screen is two separate lists and both have to name a gate before
// it can be opened:
//
//   IBs49NiH  GachaCategoryMst   the rail tile (art In7lGGLn) and, in
//                                3rCmq58M, a COMMA-SEPARATED list of the gate
//                                ids that tile pages through
//   1IR86sAv  GachaInfoMst       the gate itself (art, cost, copy)
//
// SummonsTopScene::getGachaInfoList @0x16DD398 intersects the two: it walks
// GachaInfoList and keeps the rows the CURRENT category's
// GachaCategoryMst::hasGachaID @0x1C8BD04 accepts.  A category naming a gate
// with no GachaInfo row therefore opens an empty pager, which is exactly what
// the two selector banners did before the gates were added to the archive —
// reported from the client 2026-09-12 as "selector summons are not working".
//
// Selector gates are ticket-only: SummonsDetailScene::initSummontickets
// @0x1D8E304 collects every UnitSelectorGachaMst whose gacha_gate_id matches
// the open gate, and touchBegan @0x16A73FC sends the player to the unit picker
// (scene 0x15FA0) instead of the summon when that list is non-empty.  So the
// gate needs no pool of its own; its archive pool exists only so the record is
// self-consistent.
//
// A selector banner belongs on the rail only while its ticket is held — that is
// how the live game showed them, and it keeps the rail from filling with
// sixteen gates the player can do nothing with.  gacha_category_mst.json keeps
// the permanent tiles; these are added per request, per player.

namespace gme
{

/*!
* Rail tiles for the selector tickets this player actually holds.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @return One GachaCategory per held selector ticket, in ticket-id order;
* empty when the player holds none.
*/
inline drogon::Task<std::vector<::GachaCategory>> selectorGachaCategories(
	const db::Database database,
	const UserIdentity identity)
{
	const auto rows = co_await database->execSqlCoro(
		"SELECT ticket_id, count FROM user_selector_tickets"
		" WHERE user_id = $1 AND count >= 1 ORDER BY CAST(ticket_id AS INTEGER);",
		identity.userId);

	std::vector<::GachaCategory> categories;
	for (const auto& row : rows)
	{
		const auto ticketId = row["ticket_id"].as<std::string>();

		// The selector catalog says which gate this ticket opens.
		const auto& selectors = theServer()->cache().unitSelectorGacha();
		const auto sel = std::find_if(selectors.begin(), selectors.end(),
			[&ticketId](const ::UnitSelectorGachaMst& s)
			{ return std::to_string(s.selector_id) == ticketId; });
		if (sel == selectors.end())
			continue;

		// ...and the gate has to exist, or the tile opens an empty pager.
		const auto gate = GachaArchiver::instance().lookup(
			static_cast<uint32_t>(sel->gacha_id));
		if (!gate)
			continue;

		::GachaCategory category{};
		try { category.id = 100 + std::stoi(ticketId); }
		catch (const std::exception&) { continue; }
		category.img = gate->banner.value_or("gacha_hero_selector_banner.png");
		// After the permanent tiles (1-4) and before the test gates (90).
		category.disp_order = 5 + static_cast<int32_t>(categories.size());
		category.gacha_id_list = std::to_string(sel->gacha_id);
		// Always open: the client compares now against these, and truncates the
		// end to signed 32-bit (see GachaCategory in the generated packets).
		pkg::unix_to_chrono(0, category.start_date);
		pkg::unix_to_chrono(2147483647, category.end_date);
		categories.push_back(std::move(category));
	}

	co_return categories;
}

} // namespace gme
