#include "GolfRoundRepairLog.h"

#if defined(CROSSPOINT_GOLF)

#include <Logging.h>

void golfLogRoundRepairs(const GolfRound& round, const GolfValidationResult& result) {
  if (result.currentHoleReset) {
    LOG_ERR("GOLF", "Repaired current hole to 1");
  }
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    if (result.holePuttsRepaired(hole)) {
      LOG_ERR("GOLF", "Repaired hole %u putts to %u", hole + 1, round.putts[hole]);
    }
    if (result.holeIn100Repaired(hole)) {
      LOG_ERR("GOLF", "Repaired hole %u in100 to %u", hole + 1, round.in100[hole]);
    }
    if (result.holePenaltyCountRepaired(hole)) {
      LOG_ERR("GOLF", "Repaired hole %u penalty count to %u", hole + 1, round.penaltyCount[hole]);
    }
    if (result.holePenaltyEventRepaired(hole)) {
      LOG_ERR("GOLF", "Removed invalid penalty event on hole %u", hole + 1);
    }
    if (result.holePenaltyMarkerRepaired(hole)) {
      LOG_ERR("GOLF", "Removed penalty marker exceeding shots on hole %u", hole + 1);
    }
  }
}

#endif
