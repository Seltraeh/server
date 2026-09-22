#include "App.hpp"
#include "Handlers.hpp"

// VideoAdSlotsClaimBonus (29s22s49) — the green "Claim" button on the Buy Gems
// screen.
//
// ⚠ NOTE THE GROUP ID.  `29s22s49` is one character from `29slks49`
// (UserGemShardInfo, the screen this button sits on) and its key `93055d2i` is
// one character from `930sDd3i`.  They are different requests on the same
// screen.  Transcribing either by eye is a mistake waiting to happen -- both
// were read out of the binary, never typed.
//
// Video ads are off (`video_ads` and `video_ads_slots` are both 0 in
// features.json) and there is no ad provider, so there is no bonus to claim and
// nothing here is built.  But the button is drawn regardless, and an
// unregistered GroupId answers with GmeErrorCommand::Close -- a disconnect
// rather than a refusal.  That is the fourth instance of this same shape found
// in two client runs (Shop, gifts, Arena, and now this), which is why the
// unregistered list is worth working through ahead of the player rather than
// behind them.
//
// ⚠ AN ERROR REPLY WOULD CLOSE THE SESSION TOO, so "politely refuse" has to
// mean SUCCESS with a benign body.
HANDLEF(VideoAdSlotsClaimBonus)
{
	(void)session;
	LOG_INFO << "VideoAdSlotsClaimBonus: " << json;

	co_return HandleResult::success("{}");
}
