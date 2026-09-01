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
uint8_t golfWorstHoles(const GolfRound& round, const GolfPlayerScore& score, GolfWorstHole* holes,
                       uint8_t capacity);
