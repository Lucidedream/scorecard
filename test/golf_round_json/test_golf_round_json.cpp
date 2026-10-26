#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include "GolfJson.h"
#include "GolfPaths.h"
#include "GolfRound.h"
#include "GolfRoundDecode.h"
#include "GolfStats.h"

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

TEST(GolfRoundNines, RoundTripSwappedForStateAndArchive) {
  auto original = makeFixtureRound();
  original.nines[0] = 2;
  original.nines[1] = 0;
  JsonDocument doc;
  serializeRound(original, 6, doc);
  golfAddJsonHoleArray(doc, "nines", original.nines, 2);
  for (bool state : {false, true}) {
    GolfRound out{};
    ASSERT_EQ(decode(doc, state, out), GolfRoundDecodeStatus::Ok);
    EXPECT_EQ(out.nines[0], 2);
    EXPECT_EQ(out.nines[1], 0);
    EXPECT_EQ(golfCourseHoleNumber(out, 0), 19);
  }
}
TEST(GolfRoundNines, MissingFieldDefaultsToIdentityForEveryVersion) {
  for (int version : {4, 5, 6}) {
    auto original = makeFixtureRound();
    JsonDocument doc;
    serializeRound(original, version, doc);
    for (int holes : {9, 18}) {
      doc["holes"] = holes;
      doc["par"].as<JsonArray>().clear();
      doc["si"].as<JsonArray>().clear();
      for (int h = 0; h < holes; ++h) {
        doc["par"].as<JsonArray>().add(4);
        doc["si"].as<JsonArray>().add(0);
      }
      for (auto p : doc["players"].as<JsonArray>())
        for (const char* key : {"yards", "putts", "in100", "out100", "penalties", "fairways", "bunkers"}) {
          auto arr = p[key].as<JsonArray>();
          while (arr.size() > static_cast<size_t>(holes)) arr.remove(arr.size() - 1);
          while (!arr.isNull() && arr.size() < static_cast<size_t>(holes)) {
            if (strcmp(key, "penalties") == 0)
              arr.add<JsonArray>();
            else
              arr.add(0);
          }
        }
      GolfRound out{};
      ASSERT_EQ(decode(doc, false, out), GolfRoundDecodeStatus::Ok);
      EXPECT_EQ(out.nines[0], 0);
      EXPECT_EQ(out.nines[1], holes == 9 ? GOLF_NO_NINE : 1);
    }
  }
}
TEST(GolfRoundNines, OneNineRoundTrip) {
  auto original = makeFixtureRound();
  original.holeCount = 9;
  original.nines[0] = 3;
  original.nines[1] = GOLF_NO_NINE;
  JsonDocument doc;
  serializeRound(original, 6, doc);
  golfAddJsonHoleArray(doc, "nines", original.nines, 1);
  GolfRound out{};
  ASSERT_EQ(decode(doc, false, out), GolfRoundDecodeStatus::Ok);
  EXPECT_EQ(out.nines[0], 3);
  EXPECT_EQ(out.nines[1], GOLF_NO_NINE);
}
TEST(GolfRoundNines, RejectsMalformedPresentField) {
  auto original = makeFixtureRound();
  JsonDocument doc;
  serializeRound(original, 6, doc);
  for (const char* json :
       {"null", "0", "[]", "[0]", "[0,1,2]", "[0,0]", "[-1,1]", "[0,4]", "[0,255]", "[0,1.5]", "[0,\"1\"]"}) {
    JsonDocument value;
    ASSERT_FALSE(deserializeJson(value, json));
    doc["nines"].set(value.as<JsonVariantConst>());
    GolfRound out{};
    EXPECT_NE(decode(doc, false, out), GolfRoundDecodeStatus::Ok) << json;
  }
}

TEST(GolfRoundJson, AnsweredRoundTripAndLegacyFir) {
  GolfRound original = makeFixtureRound();
  JsonDocument legacy;
  serializeRound(original, 6, legacy);
  GolfRound decoded{};
  ASSERT_EQ(decode(legacy, false, decoded), GolfRoundDecodeStatus::Ok);
  EXPECT_EQ(golfFairwaysHit(decoded, decoded.players[0].score), golfFairwaysHit(original, original.players[0].score));
  EXPECT_EQ(golfFairwaysAnswered(decoded, decoded.players[0].score),
            golfFairwaysEligible(original, original.players[0].score));
  EXPECT_EQ(golfFairwayState(decoded.players[0].score, 8), GolfFairwayState::None);
  auto& score = original.players[0].score;
  golfSetFairwayState(score, 0, GolfFairwayState::Hit);
  golfSetFairwayState(score, 1, GolfFairwayState::Missed);
  golfSetFairwayState(score, 2, GolfFairwayState::None);
  JsonDocument current;
  serializeRound(original, 6, current);
  current.remove("players");
  golfAddJsonPlayers(current, original);
  ASSERT_EQ(decode(current, true, decoded), GolfRoundDecodeStatus::Ok);
  EXPECT_EQ(golfFairwayState(decoded.players[0].score, 0), GolfFairwayState::Hit);
  EXPECT_EQ(golfFairwayState(decoded.players[0].score, 1), GolfFairwayState::Missed);
  EXPECT_EQ(golfFairwayState(decoded.players[0].score, 2), GolfFairwayState::None);
  EXPECT_EQ(golfFairwaysHit(decoded, decoded.players[0].score), 1);
  EXPECT_EQ(golfFairwaysAnswered(decoded, decoded.players[0].score), 2);
  for (const char* malformed : {"null", "[]", "[0,1]", "\"bad\""}) {
    JsonDocument value;
    ASSERT_FALSE(deserializeJson(value, malformed));
    current["players"][0]["fairwaysAnswered"] = value.as<JsonVariantConst>();
    EXPECT_NE(decode(current, false, decoded), GolfRoundDecodeStatus::Ok);
  }
  golfAddJsonPlayers(current, original);
  current["players"][0]["fairwaysAnswered"][0] = 2;
  EXPECT_NE(decode(current, false, decoded), GolfRoundDecodeStatus::Ok);
}

TEST(GolfRoundJson, FairwayCycleAndPromptEligibility) {
  GolfRound round = makeFixtureRound();
  auto& score = round.players[0].score;
  golfSetFairwayState(score, 0, GolfFairwayState::None);
  EXPECT_FALSE(golfFairwayHit(score, 0));
  for (auto expected : {GolfFairwayState::Hit, GolfFairwayState::Missed, GolfFairwayState::None}) {
    golfSetFairwayState(score, 0, golfNextFairwayState(golfFairwayState(score, 0)));
    EXPECT_EQ(golfFairwayState(score, 0), expected);
    EXPECT_EQ(golfFairwayHit(score, 0), expected == GolfFairwayState::Hit);
  }
  golfSetFairwayState(score, 18, GolfFairwayState::Hit);
  EXPECT_EQ(golfFairwayState(score, 18), GolfFairwayState::None);
  uint8_t prompted[3]{};
  golfSetTee(round.players[2], "White");
  round.players[2].score.in100[0] = 4;
  EXPECT_EQ(golfFairwayPromptPlayers(round, 0, prompted), 5);
  golfSetFairwayState(round.players[2].score, 0, GolfFairwayState::Missed);
  EXPECT_EQ(golfFairwayPromptPlayers(round, 0, prompted), 1);
  prompted[0] = 1;
  EXPECT_EQ(golfFairwayPromptPlayers(round, 0, prompted), 0);
  prompted[0] = 0;
  round.par[0] = 3;
  EXPECT_EQ(golfFairwayPromptPlayers(round, 0, prompted), 0);
  EXPECT_EQ(golfFairwayPromptPlayers(round, 8, prompted), 0);
  memset(round.par, 0, sizeof(round.par));
  EXPECT_EQ(golfFairwayPromptPlayers(round, 0, prompted), 1);
  EXPECT_EQ(golfFairwaysAnswered(round, score), 0);
}
