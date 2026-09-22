#include "App.hpp"
#include "Handlers.hpp"

// UserEnteredFeature (983D5Dii) — "the player just opened this feature".
//
// ⚠ THIS REQUEST ONLY EXISTS BECAUSE FEATURE GATING IS ON.  Turning the master
// switch `a37D29iJ` from 0 to 1 is what padlocked Raid and Guild, and it also
// activated the gating code path in every scene that consults it —
// RandallTownScene among them.  Those paths report each feature the player
// enters, so a switch that had been off since the server was written suddenly
// put a brand-new GroupId on the wire.  Unregistered, that is
// GmeErrorCommand::Close, which is why Randall started refusing to open the
// moment the padlocks started working.
//
// The lesson is the switch, not the request: enabling a client-side system
// wholesale makes every request that system owns reachable at once.  The full
// list of what is still unregistered is 47 non-deferred classes; this is simply
// the first one a player walks into.
//
// WHAT IT IS FOR: UserEnteredFeatureList (2386Diw1) is what clears the NEW
// badge on a freshly unlocked feature — see FeatureGatingInfo.new_flg in
// net/user.kdl, which notes the badge is sticky server-side once set.
// Answering `{}` keeps Randall open and leaves the badge showing, which is why
// Vortex and the Imperial Capital still say NEW.
//
// TO FINISH IT: persist the reported feature id per user and return the list
// under `2386Diw1` (UserEnteredFeatureListResponse takes e63D1BV0 feature_id,
// MHx05sXt dungeon_id, Diwl3b56 — all inlined stores at +0x18/+0x1c/+0x20).
// Then the NEW badges clear on first visit, as they did in the live game.
HANDLEF(UserEnteredFeature)
{
	(void)session;
	LOG_INFO << "UserEnteredFeature: " << json;

	// Deliberately empty rather than absent: the client asked, and an answer it
	// can parse is what keeps Randall reachable instead of killing the session.
	co_return HandleResult::success("{}");
}
