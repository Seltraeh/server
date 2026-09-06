#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

#include <sstream>
#include <string>
#include <vector>

// ItemSphereEqp (0IXGiC9t) — equip/unequip spheres (equipment items) on units
// from the sphere menu. Request (ItemSphereEqpRequest::createBody,
// bfdata/createbody/ItemSphereEqpRequest.txt):
//
//   "wx1ZLFj9": [{"a2utCvs8": "wh1:wh2:userUnitId:itemId1:itemId2[,...]"}]
//
// Each comma segment updates one unit's two sphere slots; itemId 0 clears a
// slot. wh1/wh2 are the client's warehouse instance ids (informational — the
// authoritative join is the master item id). The frame id stored alongside
// each slot is ItemMst.sphere_type, the sphere-category icon the client
// renders on the unit (legacy MstConfig::GetItemSphereType).
//
// Warehouse accounting mirrors the legacy handler: equipping consumes one from
// the sphere's stack, unequipping returns it. Rows are decremented to 0 rather
// than deleted so instance ids stay stable for the client's local warehouse
// model; UserInfo filters zero stacks off the wire.
//
// Response: empty OK (legacy-verified — the client applies the change from its
// own state).
//
// GroupId = "0IXGiC9t", AES key = "CZE56XAY" (legacy ItemSphereEqpRequestHandler).
HANDLEF(ItemSphereEqp)
{
	(void)session;
	LOG_INFO << "ItemSphereEqp: " << json;

	ItemSphereEqpReq req = {};
	if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		const auto& fmte = glz::format_error(ec, json);
		LOG_DEBUG << "Gme ItemSphereEqp Error during JSON read: " << fmte;
		co_return HandleResult::error("Deserialization error", fmte);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
	const std::string userId = identity.userId;

	const auto& itemMst = theServer()->cache().itemMst();
	const auto sphereFrame = [&itemMst](uint32_t itemId) -> int32_t {
		if (itemId == 0)
			return 0;
		for (const auto& m : itemMst)
		{
			if (m.id == static_cast<int32_t>(itemId))
				return m.sphere_type;
		}
		LOG_WARN << "ItemSphereEqp: item " << itemId << " not in ItemMst — frame defaults to 0";
		return 0;
	};

	// user_items stack adjustment for one slot change: return the previously
	// equipped sphere (UPSERT +1), consume the newly equipped one (-1, floored
	// at 0, row kept so the instance id survives).
	const auto adjustWarehouse = [&](uint32_t oldId, uint32_t newId) -> drogon::Task<void> {
		if (oldId == newId)
			co_return;
		if (oldId != 0)
		{
			co_await gme::addUserItem(theDb(), identity, oldId, 1);
		}
		if (newId != 0)
		{
			co_await theDb()->execSqlCoro(
				"UPDATE user_items SET item_num = MAX(0, item_num - 1) "
				"WHERE user_id = $1 AND item_id = $2;",
				userId, newId);
		}
	};

	for (const auto& node : req.nodes)
	{
		// packed = "wh1:wh2:userUnitId:itemId1:itemId2[,wh1:wh2:...]"
		std::istringstream ops(node.packed);
		std::string segment;
		while (std::getline(ops, segment, ','))
		{
			if (segment.empty())
				continue;

			std::vector<std::string> parts;
			std::istringstream seg(segment);
			std::string token;
			while (std::getline(seg, token, ':'))
				parts.push_back(token);

			if (parts.size() < 5)
			{
				LOG_WARN << "ItemSphereEqp: segment has fewer than 5 parts: " << segment;
				continue;
			}

			uint32_t userUnitId = 0, itemId1 = 0, itemId2 = 0;
			try { userUnitId = static_cast<uint32_t>(std::stoul(parts[2])); } catch (...) {}
			try { itemId1    = static_cast<uint32_t>(std::stoul(parts[3])); } catch (...) {}
			try { itemId2    = static_cast<uint32_t>(std::stoul(parts[4])); } catch (...) {}

			if (userUnitId == 0)
			{
				LOG_WARN << "ItemSphereEqp: bad user_unit_id in segment: " << segment;
				continue;
			}

			const int32_t frame1 = sphereFrame(itemId1);

			try
			{
				const auto oldRows = co_await theDb()->execSqlCoro(
					"SELECT eqip_item_id, eqip_item_id2, sphere_ext FROM user_units "
					"WHERE user_unit_id = $1 AND user_id = $2;",
					userUnitId, userId);
				if (oldRows.size() == 0)
				{
					LOG_WARN << "ItemSphereEqp: unit " << userUnitId << " not found";
					continue;
				}
				const uint32_t oldItem1 = oldRows[0]["eqip_item_id"].as<uint32_t>();
				const uint32_t oldItem2 = oldRows[0]["eqip_item_id2"].as<uint32_t>();

				// SLOT 2 EXISTS ONLY IF A SPHERE FROG GRANTED IT.  A unit is born with one
				// sphere slot; eqip_item_frame_id2 == -1 is how the client is told the second
				// one is not there (gme::kNoSecondSphereSlot).
				//
				// Two things therefore have to happen here, and BOTH were wrong before:
				//
				//   * REFUSE a slot-2 equip on a unit without the slot.  The client will not
				//     normally offer one -- it hides the slot on the same sentinel -- so this
				//     is a backstop against a stale client model or a crafted request, not a
				//     path players hit.
				//   * NEVER WRITE 0 INTO frame2 FOR A UNIT WITHOUT THE SLOT.  This is the
				//     bigger of the two: the old code wrote sphereFrame(itemId2) unconditionally,
				//     and sphereFrame(0) is 0, so merely equipping a sphere in SLOT 1 rewrote
				//     frame2 from -1 to 0 and silently handed the unit a second slot.
				//
				// // SUPERSEDES a comment that said the opposite -- "slot 2 is not gated and
				// // must not be, do not re-add a check" -- and an ItemSphereEqp gate removed
				// // on the strength of it.  That conclusion came from eight checks looking for
				// // a slot COUNT, which does not exist; the capacity is a sentinel in the
				// // frame id.  Some of those checks were also xref-absence arguments over
				// // VIRTUAL getters, where absence means nothing.  Confirmed in-client
				// // 2026-09-05; the reasoning is written out at gme::kNoSecondSphereSlot.
				const bool hasSlot2 = oldRows[0]["sphere_ext"].as<int32_t>() != 0;

				uint32_t eqItem2 = itemId2;
				if (!hasSlot2 && itemId2 != 0)
				{
					LOG_WARN << "ItemSphereEqp: unit " << userUnitId
						<< " has no second sphere slot -- refusing item " << itemId2;
					eqItem2 = 0;
				}

				// -1 when the slot is absent, otherwise the sphere's frame (0 = empty).
				const int32_t frame2 = hasSlot2 ? sphereFrame(eqItem2)
				                                : gme::kNoSecondSphereSlot;

				co_await theDb()->execSqlCoro(
					"UPDATE user_units "
					"SET eqip_item_id = $1, eqip_item_frame_id = $2, "
					"    eqip_item_id2 = $3, eqip_item_frame_id2 = $4 "
					"WHERE user_unit_id = $5 AND user_id = $6;",
					itemId1, frame1, eqItem2, frame2, userUnitId, userId);

				co_await adjustWarehouse(oldItem1, itemId1);
				co_await adjustWarehouse(oldItem2, eqItem2);

				LOG_INFO << "ItemSphereEqp: unit " << userUnitId
					<< " slot1=(" << itemId1 << ",frame=" << frame1 << ")"
					<< " slot2=(" << eqItem2 << ",frame=" << frame2 << ")";
			}
			catch (const drogon::orm::DrogonDbException& ex)
			{
				LOG_ERROR << "ItemSphereEqp: DB error for unit " << userUnitId
					<< ": " << ex.base().what();
			}
		}
	}

	co_return HandleResult::success("{}");
}
