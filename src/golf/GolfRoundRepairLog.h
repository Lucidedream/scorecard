#pragma once

#include "GolfRound.h"
#include "GolfValidate.h"

// Logs the per-hole repairs validateGolfRound() applied while loading a round,
// shared by the state-file loader and the completed-round reader. RoundArchive
// keeps its own "Archive repaired ..." wording for the write path.
void golfLogRoundRepairs(const GolfRound& round, const GolfValidationResult& result);
