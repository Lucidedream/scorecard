#include "GolfConfirm.h"

#if defined(CROSSPOINT_GOLF)

GolfConfirmAction golfConfirmPress(const GolfField focusedField, const bool holeLogged,
                                   const bool hasValuesToCommit) {
  if (focusedField != GolfField::Out100) return GolfConfirmAction::CycleFocus;
  if (!holeLogged && hasValuesToCommit) return GolfConfirmAction::CommitAndAdvance;
  return GolfConfirmAction::AdvanceWithoutCommit;
}

#endif
