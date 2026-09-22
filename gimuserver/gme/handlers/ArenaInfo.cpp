#include "App.hpp"
#include "Handlers.hpp"

// ArenaInfo (oim9TU1D) — the Arena entry request, and until now an
// unregistered GroupId behind a reachable button.
//
// Arena is DEFERRED by decision: it is to become a simulated online experience
// later, and none of it is built.  The intended answer is therefore a padlock,
// not a reply — and that padlock now works: `a37D29iJ` (the feature-gate master
// switch) is sent as 1, and `challenge_arena` is 0 in features.json, which are
// the two conditions HomeScene2::initialize @0x16E99DC requires before it will
// even consult shouldGateLocked(5).
//
// This handler exists for the case where something reaches the request anyway.
// An unregistered GroupId is answered with GmeErrorCommand::Close, which drops
// the session and reads as a hard crash — the same failure the Shop tab and the
// gift screen both had.  A deferred feature should be a locked door, never a
// disconnect.
//
// ⚠ AN ERROR REPLY WOULD CLOSE THE SESSION TOO.  The dispatcher turns both an
// unregistered id AND a handler error into Close, so "politely refuse" has to
// mean SUCCESS with a benign body.
//
// WHEN ARENA IS BUILT: fill UserArenaInfo (9 fields) and ChallengeArenaUserInfo
// (9 more), and drop feature ids 5 and 15 from gme::featureGates().
HANDLEF(ArenaInfo)
{
	(void)session;
	LOG_INFO << "ArenaInfo: " << json;

	// Deliberately empty rather than absent: the client asked, and an answer it
	// can parse keeps the session alive instead of killing it.
	co_return HandleResult::success("{}");
}
