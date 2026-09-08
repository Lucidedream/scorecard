#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include "GolfJson.h"
#include "GolfPaths.h"
#include "GolfRound.h"
#include "GolfRoundDecode.h"

namespace {

// Mirrors GolfRoundStore::toJson (and, for version 4, the pre-fairway wire
// shape) closely enough to exercise the real GolfJson.h codec end to end.
void serializeRound(const GolfRound& round, const int version, JsonDocument& doc) {
  doc["v"] = version;
  char date[GOLF_DATE_BUFFER_SIZE];
  if (golfFormatDate(round.dateYmd, date, sizeof(date))) {
    doc["date"] = date;
  } else {
    doc["date"] = nullptr;
  }
  doc["course"] = round.courseName;
  doc["holes"] = round.holeCount;
  doc["currentHole"] = round.currentHole;
  doc["currentPlayer"] = round.currentPlayer;
  golfAddJsonHoleArray(doc, "par", round.par, round.holeCount);
  doc["hasSi"] = round.hasSi;
  golfAddJsonHoleArray(doc, "si", round.si, round.holeCount, !round.hasSi);

  JsonArray players = doc["players"].to<JsonArray>();
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    const GolfPlayer& player = round.players[slot];
    const bool disabled = !golfPlayerIsEnabled(player);
    JsonObject encoded = players.add<JsonObject>();
    encoded["name"] = player.name;
    encoded["tee"] = player.tee;
    golfAddJsonHoleArray(encoded, "yards", player.yards, round.holeCount, disabled);
    golfAddJsonHoleArray(encoded, "putts", player.score.putts, round.holeCount, disabled);
    golfAddJsonHoleArray(encoded, "in100", player.score.in100, round.holeCount, disabled);
    golfAddJsonHoleArray(encoded, "out100", player.score.out100, round.holeCount, disabled);
    if (version >= 5) golfAddJsonFairways(encoded, player.score, round.holeCount, disabled);
    if (version >= 6) golfAddJsonBunkers(encoded, player.score, round.holeCount, disabled);
    golfAddJsonPenalties(encoded, player.score, round.holeCount, disabled);
  }
}

GolfRound makeFixtureRound() {
  GolfRound round{};
  initializeGolfPlayerDefaults(round);
  round.holeCount = 18;
  round.hasSi = false;
  round.dateYmd = 0;
  std::snprintf(round.courseName, sizeof(round.courseName), "Test Course");
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 4;
  golfSetTee(round.players[0], "Blue");

  GolfPlayerScore& score = round.players[0].score;
  for (uint8_t hole = 0; hole < 3; ++hole) {
    score.in100[hole] = 3;
    score.out100[hole] = 1;
    score.putts[hole] = 2;
  }
  golfSetFairwayHit(score, 0, true);
  golfSetFairwayHit(score, 2, true);
  golfSetFairwayHit(score, 9, true);
  golfSetGreensideBunker(score, 1, true);
  golfSetGreensideBunker(score, 2, true);
  return round;
}

GolfRoundDecodeStatus decode(const JsonDocument& doc, const bool stateFile, GolfRound& out) {
  GolfValidationResult validation{};
  return golfDecodeRoundJson(doc.as<JsonVariantConst>(), stateFile, out, validation);
}

TEST(GolfRoundJson, V5RoundFileRoundTripsFairwayBits) {
  const GolfRound original = makeFixtureRound();
  JsonDocument doc;
  serializeRound(original, 5, doc);

  GolfRound decoded{};
  ASSERT_EQ(decode(doc, false, decoded), GolfRoundDecodeStatus::Ok);
  EXPECT_EQ(decoded.holeCount, 18);
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    EXPECT_EQ(golfFairwayHit(decoded.players[0].score, hole), golfFairwayHit(original.players[0].score, hole))
        << "hole " << int{hole};
  }
  EXPECT_TRUE(golfFairwayHit(decoded.players[0].score, 0));
  EXPECT_TRUE(golfFairwayHit(decoded.players[0].score, 2));
  EXPECT_TRUE(golfFairwayHit(decoded.players[0].score, 9));
  EXPECT_FALSE(golfFairwayHit(decoded.players[0].score, 1));
}

TEST(GolfRoundJson, V5StateRoundTripsFairwayBitsAndCursor) {
  GolfRound original = makeFixtureRound();
  original.currentHole = 3;
  original.currentPlayer = 0;
  JsonDocument doc;
  serializeRound(original, 5, doc);

  GolfRound decoded{};
  ASSERT_EQ(decode(doc, true, decoded), GolfRoundDecodeStatus::Ok);
  EXPECT_EQ(decoded.currentHole, 3);
  EXPECT_EQ(decoded.currentPlayer, 0);
  EXPECT_TRUE(golfFairwayHit(decoded.players[0].score, 0));
  EXPECT_TRUE(golfFairwayHit(decoded.players[0].score, 9));
}

TEST(GolfRoundJson, V4FileLoadsWithBitsClearAndReserializesAsV5) {
  GolfRound source = makeFixtureRound();
  // A genuine v4 file never carried fairway data.
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    golfSetFairwayHit(source.players[0].score, hole, false);
  }
  JsonDocument v4doc;
  serializeRound(source, 4, v4doc);
  ASSERT_FALSE(v4doc["players"][0]["fairways"].is<JsonArrayConst>());

  GolfRound decoded{};
  ASSERT_EQ(decode(v4doc, false, decoded), GolfRoundDecodeStatus::Ok);
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    EXPECT_FALSE(golfFairwayHit(decoded.players[0].score, hole));
  }

  JsonDocument v5doc;
  serializeRound(decoded, 5, v5doc);
  EXPECT_EQ(v5doc["v"].as<int>(), 5);
  ASSERT_TRUE(v5doc["players"][0]["fairways"].is<JsonArray>());
  EXPECT_EQ(v5doc["players"][0]["fairways"].as<JsonArrayConst>().size(), 18u);
  for (JsonVariantConst value : v5doc["players"][0]["fairways"].as<JsonArrayConst>()) {
    EXPECT_EQ(value.as<int>(), 0);
  }
  // A disabled slot still gets a zero-filled array.
  EXPECT_EQ(v5doc["players"][3]["fairways"].as<JsonArrayConst>().size(), 18u);

  GolfRound reDecoded{};
  EXPECT_EQ(decode(v5doc, false, reDecoded), GolfRoundDecodeStatus::Ok);
}

TEST(GolfRoundJson, V5RejectsWrongLengthFairwayArray) {
  const GolfRound original = makeFixtureRound();
  JsonDocument doc;
  serializeRound(original, 5, doc);
  doc["players"][0]["fairways"].as<JsonArray>().add(0);  // 19 entries

  GolfRound decoded{};
  EXPECT_EQ(decode(doc, false, decoded), GolfRoundDecodeStatus::RejectedArrayLength);
}

TEST(GolfRoundJson, V5RejectsOutOfRangeFairwayValue) {
  const GolfRound original = makeFixtureRound();
  JsonDocument doc;
  serializeRound(original, 5, doc);
  doc["players"][0]["fairways"][1] = 2;

  GolfRound decoded{};
  EXPECT_EQ(decode(doc, false, decoded), GolfRoundDecodeStatus::RejectedMetadata);
}

TEST(GolfRoundJson, V5RejectsFairwayBitOnDisabledSlot) {
  const GolfRound original = makeFixtureRound();
  JsonDocument doc;
  serializeRound(original, 5, doc);
  doc["players"][3]["fairways"][0] = 1;  // player 3 is NotPlay

  GolfRound decoded{};
  EXPECT_EQ(decode(doc, false, decoded), GolfRoundDecodeStatus::RejectedDisabledPlayerData);
}

TEST(GolfRoundJson, V1RemainsRejected) {
  const GolfRound original = makeFixtureRound();
  JsonDocument doc;
  serializeRound(original, 1, doc);

  GolfRound decoded{};
  EXPECT_EQ(decode(doc, false, decoded), GolfRoundDecodeStatus::RejectedVersion);
}

TEST(GolfRoundJson, V6RoundFileRoundTripsFreeFormTeeAndBunkerBits) {
  GolfRound original = makeFixtureRound();
  golfSetTee(original.players[0], "Back 9");
  JsonDocument doc;
  serializeRound(original, 6, doc);

  GolfRound decoded{};
  ASSERT_EQ(decode(doc, false, decoded), GolfRoundDecodeStatus::Ok);
  EXPECT_STREQ(decoded.players[0].tee, "Back 9");
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    EXPECT_EQ(golfGreensideBunker(decoded.players[0].score, hole), golfGreensideBunker(original.players[0].score, hole))
        << "hole " << int{hole};
    EXPECT_EQ(golfFairwayHit(decoded.players[0].score, hole), golfFairwayHit(original.players[0].score, hole));
  }
  EXPECT_TRUE(golfGreensideBunker(decoded.players[0].score, 1));
  EXPECT_TRUE(golfGreensideBunker(decoded.players[0].score, 2));
  EXPECT_FALSE(golfGreensideBunker(decoded.players[0].score, 0));
}

TEST(GolfRoundJson, V6StateRoundTripsBunkerBitsAndCursor) {
  GolfRound original = makeFixtureRound();
  original.currentHole = 3;
  original.currentPlayer = 0;
  JsonDocument doc;
  serializeRound(original, 6, doc);

  GolfRound decoded{};
  ASSERT_EQ(decode(doc, true, decoded), GolfRoundDecodeStatus::Ok);
  EXPECT_EQ(decoded.currentHole, 3);
  EXPECT_TRUE(golfGreensideBunker(decoded.players[0].score, 2));
}

TEST(GolfRoundJson, V5FileLoadsWithBunkerBitsClearThenReserializesAsV6) {
  const GolfRound source = makeFixtureRound();
  JsonDocument v5doc;
  serializeRound(source, 5, v5doc);
  ASSERT_FALSE(v5doc["players"][0]["bunkers"].is<JsonArrayConst>());

  GolfRound decoded{};
  ASSERT_EQ(decode(v5doc, false, decoded), GolfRoundDecodeStatus::Ok);
  EXPECT_STREQ(decoded.players[0].tee, "Blue");
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    EXPECT_FALSE(golfGreensideBunker(decoded.players[0].score, hole));
  }

  JsonDocument v6doc;
  serializeRound(decoded, 6, v6doc);
  EXPECT_EQ(v6doc["v"].as<int>(), 6);
  ASSERT_TRUE(v6doc["players"][0]["bunkers"].is<JsonArray>());
  EXPECT_EQ(v6doc["players"][0]["bunkers"].as<JsonArrayConst>().size(), 18u);
  EXPECT_EQ(v6doc["players"][3]["bunkers"].as<JsonArrayConst>().size(), 18u);  // disabled slot zero-filled

  GolfRound reDecoded{};
  EXPECT_EQ(decode(v6doc, false, reDecoded), GolfRoundDecodeStatus::Ok);
}

TEST(GolfRoundJson, V6RejectsWrongLengthBunkerArray) {
  const GolfRound original = makeFixtureRound();
  JsonDocument doc;
  serializeRound(original, 6, doc);
  doc["players"][0]["bunkers"].as<JsonArray>().add(0);  // 19 entries

  GolfRound decoded{};
  EXPECT_EQ(decode(doc, false, decoded), GolfRoundDecodeStatus::RejectedArrayLength);
}

TEST(GolfRoundJson, V6RejectsOutOfRangeBunkerValue) {
  const GolfRound original = makeFixtureRound();
  JsonDocument doc;
  serializeRound(original, 6, doc);
  doc["players"][0]["bunkers"][1] = 2;

  GolfRound decoded{};
  EXPECT_EQ(decode(doc, false, decoded), GolfRoundDecodeStatus::RejectedMetadata);
}

TEST(GolfRoundJson, V6RejectsBunkerBitOnDisabledSlot) {
  const GolfRound original = makeFixtureRound();
  JsonDocument doc;
  serializeRound(original, 6, doc);
  doc["players"][3]["bunkers"][0] = 1;  // player 3 did not play

  GolfRound decoded{};
  EXPECT_EQ(decode(doc, false, decoded), GolfRoundDecodeStatus::RejectedDisabledPlayerData);
}

TEST(GolfRoundJson, V6RejectsMalformedTee) {
  const GolfRound original = makeFixtureRound();

  JsonDocument tooLong;
  serializeRound(original, 6, tooLong);
  tooLong["players"][0]["tee"] = "TwelveCharsX";  // 12 chars, no room for NUL
  GolfRound a{};
  EXPECT_EQ(decode(tooLong, false, a), GolfRoundDecodeStatus::RejectedMetadata);

  JsonDocument comma;
  serializeRound(original, 6, comma);
  comma["players"][0]["tee"] = "Front,Back";
  GolfRound b{};
  EXPECT_EQ(decode(comma, false, b), GolfRoundDecodeStatus::RejectedMetadata);
}

TEST(GolfRoundJson, V6TeeCopiesStraightInFromV5Fixture) {
  GolfRound source = makeFixtureRound();
  golfSetTee(source.players[0], "White");
  JsonDocument v5doc;
  serializeRound(source, 5, v5doc);

  GolfRound decoded{};
  ASSERT_EQ(decode(v5doc, false, decoded), GolfRoundDecodeStatus::Ok);
  EXPECT_STREQ(decoded.players[0].tee, "White");
}

}  // namespace
