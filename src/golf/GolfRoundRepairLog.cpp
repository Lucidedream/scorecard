#include "GolfRoundRepairLog.h"

#if defined(CROSSPOINT_GOLF)

#include <Logging.h>

void golfLogRoundRepairs(const GolfRound& round, const GolfValidationResult& result) {
  if (result.currentHoleReset) LOG_ERR("GOLF", "Repaired current hole to 1");
  if (result.currentPlayerReset) {
    LOG_ERR("GOLF", "Repaired current player to slot %u", static_cast<unsigned>(round.currentPlayer));
  }
  if (result.firstPlayerEnabled) LOG_ERR("GOLF", "Repaired player 1 tee to Blue");
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    const GolfPlayerScore& score = round.players[slot].score;
    const GolfPlayerValidationResult& repaired = result.players[slot];
    for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
      if (repaired.holePuttsRepaired(hole)) {
        LOG_ERR("GOLF", "Repaired player %u hole %u putts to %u", slot, hole + 1, score.putts[hole]);
      }
      if (repaired.holeIn100Repaired(hole)) {
        LOG_ERR("GOLF", "Repaired player %u hole %u in100 to %u", slot, hole + 1, score.in100[hole]);
      }
      if (repaired.holePenaltyCountRepaired(hole)) {
        LOG_ERR("GOLF", "Repaired player %u hole %u penalty count to %u", slot, hole + 1, score.penaltyCount[hole]);
      }
      if (repaired.holePenaltyEventRepaired(hole)) {
        LOG_ERR("GOLF", "Removed invalid penalty event for player %u hole %u", slot, hole + 1);
      }
      if (repaired.holePenaltyMarkerRepaired(hole)) {
        LOG_ERR("GOLF", "Removed penalty marker exceeding shots for player %u hole %u", slot, hole + 1);
      }
    }
  }
}

#endif
