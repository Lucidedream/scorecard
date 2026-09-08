#include "GolfRoundDecode.h"

#if defined(CROSSPOINT_GOLF)

#include <cstring>

namespace {

bool playerPayloadIsZero(const GolfPlayer& player, const uint8_t holes) {
  for (uint8_t hole = 0; hole < holes; ++hole) {
    if (player.yards[hole] != 0 || player.score.putts[hole] != 0 || player.score.in100[hole] != 0 ||
        player.score.out100[hole] != 0 || player.score.penaltyCount[hole] != 0) {
      return false;
    }
    for (uint8_t byte = 0; byte < GolfRound::MAX_PENALTIES_PER_HOLE / 2; ++byte) {
      if (player.score.penaltyEvents[hole][byte] != 0) return false;
    }
  }
  for (uint8_t byte = 0; byte < sizeof(player.score.fairwayHit); ++byte) {
    if (player.score.fairwayHit[byte] != 0) return false;
  }
  for (uint8_t byte = 0; byte < sizeof(player.score.greensideBunker); ++byte) {
    if (player.score.greensideBunker[byte] != 0) return false;
  }
  return true;
}

// UTF-8 shape check matching the player-name predicate (singleLineUtf8): a bare
// scan for structurally valid sequences, no normalisation.
bool teeUtf8Valid(const char* value) {
  const auto* current = reinterpret_cast<const uint8_t*>(value);
  while (*current != 0) {
    if (*current < 0x80) {
      ++current;
    } else if (*current >= 0xc2 && *current <= 0xdf && (current[1] & 0xc0) == 0x80) {
      current += 2;
    } else if (*current >= 0xe0 && *current <= 0xef && (current[1] & 0xc0) == 0x80 && (current[2] & 0xc0) == 0x80 &&
               !(*current == 0xe0 && current[1] < 0xa0) && !(*current == 0xed && current[1] >= 0xa0)) {
      current += 3;
    } else if (*current >= 0xf0 && *current <= 0xf4 && (current[1] & 0xc0) == 0x80 && (current[2] & 0xc0) == 0x80 &&
               (current[3] & 0xc0) == 0x80 && !(*current == 0xf0 && current[1] < 0x90) &&
               !(*current == 0xf4 && current[1] >= 0x90)) {
      current += 4;
    } else {
      return false;
    }
  }
  return true;
}

bool sharedSiIsCanonical(const GolfRound& round, const uint8_t holes) {
  uint32_t seen = 0;
  for (uint8_t hole = 0; hole < holes; ++hole) {
    if (!round.hasSi) {
      if (round.si[hole] != 0) return false;
      continue;
    }
    const uint8_t strokeIndex = round.si[hole];
    if (strokeIndex == 0 || strokeIndex > holes) return false;
    const uint32_t bit = 1UL << (strokeIndex - 1);
    if ((seen & bit) != 0) return false;
    seen |= bit;
  }
  return true;
}

}  // namespace

bool golfTeeStringValid(const char* s) {
  if (s == nullptr || s[0] == '\0') return false;
  if (strlen(s) >= GOLF_TEE_CAPACITY) return false;
  if (strpbrk(s, ",\r\n") != nullptr) return false;
  return teeUtf8Valid(s);
}

const char* golfLegacyTeeSelection(const char* legacyTee) {
  if (legacyTee != nullptr && strcmp(legacyTee, "White") == 0) return "White";
  return "Blue";
}

void golfInitializeLegacyRound(GolfRound& round, const char* legacyTee) {
  round = {};
  initializeGolfPlayerDefaults(round);
  golfSetTee(round.players[0], golfLegacyTeeSelection(legacyTee));
  round.currentPlayer = 0;
}

GolfRoundDecodeStatus golfCheckRound(GolfRound& out, const int version, const int holes, const int currentHole,
                                     const int currentPlayer, const GolfRoundColumnLengths& lengths,
                                     GolfValidationResult& validation) {
  if (version < 2 || version > 6) {
    return GolfRoundDecodeStatus::RejectedVersion;
  }
  if (holes != 9 && holes != 18) return GolfRoundDecodeStatus::RejectedHoleCount;
  const uint8_t holeCount = static_cast<uint8_t>(holes);

  if (lengths.par != holeCount) return GolfRoundDecodeStatus::RejectedArrayLength;
  if (version >= 4) {
    if (lengths.players != GolfRound::MAX_PLAYERS) return GolfRoundDecodeStatus::RejectedPlayerCount;
    if (lengths.si != holeCount) return GolfRoundDecodeStatus::RejectedArrayLength;
    for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
      const GolfPlayerColumnLengths& player = lengths.player[slot];
      if (player.yards != holeCount || player.putts != holeCount || player.in100 != holeCount ||
          player.out100 != holeCount || player.penalties != holeCount) {
        return GolfRoundDecodeStatus::RejectedArrayLength;
      }
      if (version >= 5 && player.fairways != holeCount) return GolfRoundDecodeStatus::RejectedArrayLength;
      if (version >= 6 && player.bunkers != holeCount) return GolfRoundDecodeStatus::RejectedArrayLength;
      if (!golfPlayerIsEnabled(out.players[slot]) && !playerPayloadIsZero(out.players[slot], holeCount)) {
        return GolfRoundDecodeStatus::RejectedDisabledPlayerData;
      }
    }
    if (!sharedSiIsCanonical(out, holeCount)) return GolfRoundDecodeStatus::RejectedSharedData;
  } else {
    const GolfPlayerColumnLengths& player = lengths.player[0];
    if (player.putts != holeCount || player.in100 != holeCount || player.out100 != holeCount ||
        (lengths.expectLegacyYards && player.yards != holeCount) || (version == 3 && player.penalties != holeCount)) {
      return GolfRoundDecodeStatus::RejectedArrayLength;
    }
  }

  out.holeCount = holeCount;
  out.currentHole = currentHole < 0 || currentHole >= holes ? holeCount : static_cast<uint8_t>(currentHole);
  out.currentPlayer = currentPlayer < 0 || currentPlayer >= GolfRound::MAX_PLAYERS
                          ? GolfRound::NO_PLAYER
                          : static_cast<uint8_t>(currentPlayer);
  validation = validateGolfRound(out);
  return validation.valid ? GolfRoundDecodeStatus::Ok : GolfRoundDecodeStatus::RejectedRound;
}

#endif
