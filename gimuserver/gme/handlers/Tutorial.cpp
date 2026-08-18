#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <array>
#include <cstdint>
#include <optional>

namespace
{

// The healing potion every first-time player receives; the early tutorial
// uses it to explain usable battle items.
constexpr uint32_t kTutorialPotionItemId = 20000;

// Marks a present as belonging to the tutorial-completion set.  Doubles as the
// one-shot guard: TutorialUpdate fires repeatedly and the client keeps
// re-reporting end_flag=1, so "does a present with this receipt_type already
// exist" is what stops the gifts being queued twice.
constexpr int32_t kTutorialGiftReceiptType = 1;

// What the completion dialog pays out.
//
// present_type follows the shared reward vocabulary (wire hash 30Kw4WBa, the
// same one CampaignReceipt dispatches on): 3 = zel, 8 = gem, 6 = unit,
// 4/5/7 = item/material/sphere.  target_id is empty for currency.
//
// ⚠ These VALUES are a fork choice, not recovered data.  The live game's
// tutorial gift set is not in any MST we hold — deploy/mst/gift_item_mst.json
// is the FRIEND-gifting catalog ("Send some Zel to your friend!"), a different
// feature entirely.  The set below is deliberately modest and mirrors the
// tutorial's own economy: enough gems for one more Rare Summon (gate 2000
// costs 5), some starting zel, and a few more of the potion the tutorial
// taught the player to use.  Adjust freely — nothing here is load-bearing.
struct TutorialGift
{
	int32_t present_type;
	const char* target_id;
	int32_t target_cnt;
	const char* description;   // ZC0msu2L — the tile's display text
};

constexpr std::array<TutorialGift, 3> kTutorialCompletionGifts{{
	{ 8, "",      5,     "Tutorial Complete Bonus" },  // one more Rare Summon
	{ 3, "",      10000, "Tutorial Complete Bonus" },
	{ 4, "20000", 3,     "Tutorial Complete Bonus" },
}};

std::optional<uint32_t> getStarterUnit(uint32_t element)
{
	switch (element)
	{
	case 1:
		return 10011;
	case 2:
		return 20011;
	case 3:
		return 30011;
	case 4:
		return 40011;
	default:
		return std::nullopt;
	}
}
}

HANDLEF(NgwordCheck)
{
	co_return HandleResult::success("{}");
}

HANDLEF(CreateUser)
{
	CreateUserReq req = {};
	const auto& ec = glz::read_json(req, json);
	if (ec)
	{
		const auto error = glz::format_error(ec, json);
		LOG_ERROR << "CreateUserReq deserialization failed:\n" << error;
		co_return HandleResult::error("Deserialization error", error);
	}

	const auto& handleName = req.login_info.handle_name;
	if (handleName.empty())
	{
		co_return HandleResult::error(
			"Missing handle name",
			"CreateUser request did not include a handle name");
	}

	auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info, true)).data;
	if (!identity.userId.empty())
	{
		co_return HandleResult::error(
			"User already exists",
			"CreateUser cannot create a second user for this Gumi Live user");
	}
	identity.userId = RandomId();

	// Grab the starter units.
	const auto starterUnitId = getStarterUnit(req.selected_element.element);
	if (!starterUnitId)
	{
		co_return HandleResult::error("Invalid tutorial starter element");
	}
	auto starter = gme::fromArchivedUnit(*starterUnitId, UnitArchiver::getRandomType());
	auto burny = gme::fromArchivedUnit(10030, UnitArchiver::getRandomType());
	auto sparky = gme::fromArchivedUnit(40030, UnitArchiver::getRandomType());
	if (!starter || !burny || !sparky)
	{
		co_return HandleResult::error("Archive error", "Unable to create tutorial units from archive");
	}

	// We need to wrap these operations in a transaction to avoid an invalid
	// intermediate state. Keep this directly in the handler coroutine so local
	// packet objects remain alive across co_await suspension points.
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			// Create the new user.
			const auto levelMst = gme::getLevelMst(1);
			if (!levelMst)
			{
				throw std::runtime_error("Unable to create user without level MST");
			}

			(co_await db::DatabaseInterface::insert(
				transaction,
					"user_info",
					{
					db::Data("id", identity.userId),
					db::Data("gumi_user_id", identity.gumiUserId),
					db::Data("device_id", std::string()),
					db::Data("username", handleName),
					db::Data("level", uint32_t(1)),
					db::Data("zel", uint64_t(5000)),
					db::Data("karma", uint64_t(1000)),
					db::Data("energy", levelMst->energy),
					db::Data("energy_full_ts", uint64_t(0)),
					db::Data("max_warehouse_count", 100),
				})).nonEmpty();

			*starter = (co_await gme::addUserUnit(
				transaction,
				identity,
				*starter)).nonEmpty();

			(co_await gme::addDefaultDecks(
				transaction,
				identity,
				*starter)).nonEmpty();

			*burny = (co_await gme::addUserUnit(
				transaction,
				identity,
				*burny)).nonEmpty();

			(co_await db::PacketInterfaceFor<UserPartyDeckInfo>::insert(
				transaction,
				"user_decks",
				{
					.user_unit_id = burny->user_unit_id,
					.deck_type = 1,
					.deck_num = 0,
					.member_type = 1,
					.disp_order = 0,
				},
				{ db::Data("user_id", identity.userId) })).nonEmpty();

			*sparky = (co_await gme::addUserUnit(
				transaction,
				identity,
				*sparky)).nonEmpty();

			(co_await db::PacketInterfaceFor<UserPartyDeckInfo>::insert(
				transaction,
				"user_decks",
				{
					.user_unit_id = sparky->user_unit_id,
					.deck_type = 1,
					.deck_num = 0,
					.member_type = 1,
					.disp_order = 1,
				},
				{ db::Data("user_id", identity.userId) })).nonEmpty();

				// Every first-time player gets one healing potion (item 20000),
				// the item the early tutorial uses to explain usable battle
				// items.  Natural provisioning — replaces the old UserInfo
				// hardcode that faked this row for the tutorial account.
				(co_await gme::addUserItem(
					transaction, identity, kTutorialPotionItemId)).nonEmpty();

				// …and put it in the battle-item loadout, not just the
				// warehouse.  tuto1.txt STALLS without this: the first battle
				// runs `not_item` (item button off) → `enable_item` →
				// "Tap the cure to give it a try" → `change_item_scene:
				// tuto_use_item`, and that last step blocks until the player
				// taps a cure in the in-battle item menu.  That menu is fed by
				// UserInfo's equip_info (71U5wzhI), which is now REPORTED from
				// user_equip_items rather than synthesised from the warehouse —
				// so on a fresh save the bar is empty and the tutorial cannot
				// leave its first script.
				//
				// dev covered this by hardcoding equip_info to item 20000 on
				// every UserInfo ("to get past the tutorial, hard code it").
				// Seeding a real row keeps ItemEdit's ownership of the loadout
				// intact — the player can overwrite slot 0 at any time — while
				// still handing a new account the one item the tutorial needs.
				(co_await db::DatabaseInterface::insert(
					transaction,
					"user_equip_items",
					{
						db::Data("user_id", identity.userId),
						db::Data("disp_order", uint32_t(0)),
						db::Data("item_id", kTutorialPotionItemId),
						db::Data("item_num", uint32_t(1)),
					})).nonEmpty();

				// Provision the town up front.  Nothing else creates it, and an
				// empty town crashes the client on entry (§6.8) — including the
				// pass tuto15.txt makes through `change_town_top_scene` on its
				// way to the summon gate.  Visibility is still progression-gated
				// client-side by the cleared-mission set, so seeding the rows
				// does not hand a new player a town they haven't unlocked.
				co_await gme::provisionTown(transaction, identity);
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	co_return HandleResult::success("{}");
}

HANDLEF(TutorialSkip)
{
	// Client just needs an acknowledgment.
	TutorialSkipResp resp{};
	resp.tutorial_skip_info.ack = true;

	std::string buffer{};
	const auto& ec = glz::write_json(resp, buffer);
	if (ec)
	{
		co_return HandleResult::error("Serialization error");
	}

	co_return HandleResult::success(buffer);
}

HANDLEF(TutorialUpdate)
{
	TutorialUpdateReq req = {};
	const auto& ec = glz::read_json(req, json);
	if (ec)
	{
		const auto error = glz::format_error(ec, json);
		LOG_ERROR << "TutorialUpdateReq deserialization failed:\n" << error;
		co_return HandleResult::error("Deserialization error", error);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// Both tutorial markers travel in the login envelope and are stored
	// verbatim.  tutorial_status (9sQM2XcN) is the pointer at the next
	// client-bundled script (deploy/game_content/content/tuto/tuto<N>.txt);
	// tutorial_end_flag (sv6BEI8X) is the client's own "I am done" verdict,
	// which the server used to infer from a threshold instead of reading.
	(co_await db::DatabaseInterface::update(
		theDb(),
		"user_info",
		{
			db::Data("tutorial_status", req.login_info.tutorial_status),
			db::Data("tutorial_end_flag", req.login_info.tutorial_end_flag),
			db::Lookup("gumi_user_id", identity.gumiUserId),
			db::Lookup("id", identity.userId),
		})).nonEmpty();

	LOG_INFO << "TutorialUpdate: status=" << req.login_info.tutorial_status
		<< " end_flag=" << req.login_info.tutorial_end_flag;

	// Tutorial-completion gifts.
	//
	// On the last step the client shows "You have completed the basic tutorial,
	// Gifts have been awarded to you.  Visit your presents box to receive
	// them." — a promise only the server can keep.  Queue them the moment the
	// client reports the tutorial finished.
	//
	// Guarded by the row's own pre-update flag rather than by anything in the
	// request: TutorialUpdate fires repeatedly, and the client re-reports
	// end_flag=1 on later calls, so keying off the request alone would hand out
	// a fresh set every time.  `hadCompleted` is read BEFORE the update above
	// commits its new value... except the update has already run, so re-derive
	// the one-shot from the present box itself — if a completion gift already
	// exists for this user, there is nothing to do.
	if (req.login_info.tutorial_end_flag)
	{
		const auto existing = co_await theDb()->execSqlCoro(
			"SELECT 1 FROM user_presents WHERE user_id = $1 AND receipt_type = $2 LIMIT 1;",
			identity.userId, kTutorialGiftReceiptType);

		if (existing.empty())
		{
			for (const auto& gift : kTutorialCompletionGifts)
			{
				co_await gme::addUserPresent(
					theDb(), identity,
					gift.present_type, gift.target_id, gift.target_cnt,
					kTutorialGiftReceiptType, gift.description);
			}
			LOG_INFO << "TutorialUpdate: queued " << kTutorialCompletionGifts.size()
				<< " tutorial-completion gift(s) for " << identity.userId;
		}
	}

	co_return HandleResult::success("{}");
}
