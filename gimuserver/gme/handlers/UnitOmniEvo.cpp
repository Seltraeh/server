#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

// UnitOmniEvo (4Dk4spf9 / 4s3lsODp) — Omni+ Boost.  A PROBE, not the feature.
//
// WHY A PROBE AND NOT THE HANDLER.  Everything about Omni+ is already on disk
// and matches the Global wiki exactly:
//
//   unit_evo_omni_mst       18 rows, (cost band, +level) -> raised-stat bumps
//                           "150:60:60:60" at +1, "450:180:180:180" at +2,
//                           "900:360:360:360" at +3, plus 5/5/10 SP.
//   unit_evo_omni_type_mst  36 rows, the Zel/Karma half of the price
//                           (4,000,000 / 1,500,000 at +1 — the wiki's figures).
//   unit_evo_omni_recipe_mst 70 rows, the materials: 100 Elemental Shards and
//                           10 Elementum Tomes for a cost-60 unit at +1, or a
//                           Duplicate plus 5 Geminus Tomes.
//
// What is NOT established is which node of the request names the unit being
// BOOSTED.  createBody @0x1C6F68C sends only two groups — `34d0Slkf` carrying
// `s049sdck` (the recipe type, 1 Elemental / 2 Duplicate) and `8Z2NQrx1`
// carrying the same {id, slot, kind, count} selection nodes UnitBondBoost uses
// — and the base unit is somewhere inside that selection, distinguished by
// `29MgiJIQ`.  Guessing which kind it is would mean boosting the wrong unit or
// eating the wrong material, silently, on a request that spends four million
// Zel.  Handbook §6.16: take a sample first.
//
// So this registers the GroupId with its real key, logs the DECRYPTED body, and
// refuses.  Nothing is consumed, the session survives (an unregistered GroupId
// would close it), and one tap gives the field mapping exactly.
HANDLEF(UnitOmniEvo)
{
	(void)session;
	LOG_INFO << "UnitOmniEvo PROBE: " << json;
	co_return HandleResult::error(
		"Omni+ Boost is not built yet",
		"the request has been logged for decoding — nothing was spent");
}
