#include "App.hpp"
#include "Handlers.hpp"

// UserGemShardInfo (29slks49) — the gem screen behind the Shop's big gem
// button.
//
// Gem shards are the IAP currency fragment: buying gems is the one feature
// explicitly deferred as "no money changes hands", so nothing here is built and
// nothing should be purchasable.
//
// But the button is REACHABLE now that the Shop tab opens at all (its layout
// CSV was being served from the wrong folder until this round), and an
// unregistered GroupId answers with GmeErrorCommand::Close — a disconnect, not
// a refusal.  That is the same failure the Shop tab, the gift screen and Arena
// each had: a deferred feature should be an empty screen or a padlock, never a
// dropped session.
//
// ⚠ AN ERROR REPLY WOULD CLOSE THE SESSION TOO.  The dispatcher turns both an
// unregistered id AND a handler error into Close, so "politely refuse" has to
// mean SUCCESS with a benign body.
//
// WHEN IAP IS RECONSIDERED: the plan on record is free one-time "purchases"
// cycled by the server rather than real payments, which would fill this plus
// BundlePurchase (D3gyT3b3) and BundleCategoryRefresh (swarOb4u).  All three
// are unregistered today.
HANDLEF(UserGemShardInfo)
{
	(void)session;
	LOG_INFO << "UserGemShardInfo: " << json;

	co_return HandleResult::success("{}");
}
