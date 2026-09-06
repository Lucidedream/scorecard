#pragma once

#include <cstdint>

#include "GolfRound.h"

struct GolfWorstHole {
  uint8_t hole;
  int16_t toPar;
};

uint8_t golfLongGame(const GolfRound& round, const GolfPlayerScore& score, uint8_t hole);
uint16_t golfPenaltyTotal(const GolfRound& round, const GolfPlayerScore& score);
uint16_t golfHoleScore(const GolfRound& round, const GolfPlayerScore& score, uint8_t hole);
uint16_t golfScore(const GolfRound& round, const GolfPlayerScore& score);
uint16_t golfParTotal(const GolfRound& round, const GolfPlayerScore& score);
bool golfHasPar(const GolfRound& round);
int16_t golfToPar(const GolfRound& round, const GolfPlayerScore& score);
uint8_t golfThru(const GolfRound& round, const GolfPlayerScore& score);
uint16_t golfPuttsTotal(const GolfRound& round, const GolfPlayerScore& score);
uint16_t golfIn100Total(const GolfRound& round, const GolfPlayerScore& score);
uint16_t golfShortTotal(const GolfRound& round, const GolfPlayerScore& score);
uint16_t golfLongTotal(const GolfRound& round, const GolfPlayerScore& score);
uint8_t golfOnePutts(const GolfRound& round, const GolfPlayerScore& score);
uint8_t golfThreePutts(const GolfRound& round, const GolfPlayerScore& score);
uint8_t golfWorstHoles(const GolfRound& round, const GolfPlayerScore& score, GolfWorstHole* holes, uint8_t capacity);

// Green in regulation: the hole is entered, its par is 3..5, and the strokes to
// reach the green (holeScore - putts) are at most par - 2. Par 6 and par-free
// holes have no GIR (CONTRACTS-V2 §31.4).
bool golfGreenInRegulation(const GolfRound& round, const GolfPlayerScore& score, uint8_t hole);
// Count of entered holes meeting golfGreenInRegulation.
uint8_t golfGreensInRegulation(const GolfRound& round, const GolfPlayerScore& score);
// GIR denominator: entered holes whose par is in [3, 5].
uint8_t golfGreensEligible(const GolfRound& round, const GolfPlayerScore& score);
// FIR numerator: entered holes with the fairway bit set (not re-filtered by par).
uint8_t golfFairwaysHit(const GolfRound& round, const GolfPlayerScore& score);
// FIR denominator: entered par-4/5 holes; 0 on a par-free round (golfHasPar false).
uint8_t golfFairwaysEligible(const GolfRound& round, const GolfPlayerScore& score);
