#pragma once

#include "GolfRound.h"
#include "GolfValidate.h"

// Logs cursor and per-player/per-hole repairs applied while loading a round.
// Shared by the state-file loader, completed-round reader, and archive path.
void golfLogRoundRepairs(const GolfRound& round, const GolfValidationResult& result);
