#pragma once

#if defined(CROSSPOINT_GOLF)

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "GolfPaths.h"
#include "GolfPenalty.h"
#include "GolfRoundDecode.h"

inline bool golfJsonValidUtf8(const char* value) {
  if (value == nullptr) return false;
  const auto* current = reinterpret_cast<const uint8_t*>(value);
  while (*current != 0) {
    if (*current < 0x80) {
      ++current;
    } else if (*current >= 0xc2 && *current <= 0xdf && (current[1] & 0xc0) == 0x80) {
      current += 2;
    } else if (*current >= 0xe0 && *current <= 0xef && (current[1] & 0xc0) == 0x80 &&
               (current[2] & 0xc0) == 0x80 && !(*current == 0xe0 && current[1] < 0xa0) &&
               !(*current == 0xed && current[1] >= 0xa0)) {
      current += 3;
    } else if (*current >= 0xf0 && *current <= 0xf4 && (current[1] & 0xc0) == 0x80 &&
               (current[2] & 0xc0) == 0x80 && (current[3] & 0xc0) == 0x80 &&
               !(*current == 0xf0 && current[1] < 0x90) && !(*current == 0xf4 && current[1] >= 0x90)) {
      current += 4;
    } else {
      return false;
    }
  }
  return true;
}

inline bool golfReadJsonString(const JsonVariantConst value, char* output, const size_t capacity,
                               const bool requireUtf8 = false) {
  if (!value.is<const char*>()) return false;
  const char* source = value.as<const char*>();
  const size_t length = strlen(source);
  if (length >= capacity ||
      (requireUtf8 && (strpbrk(source, "\r\n") != nullptr || !golfJsonValidUtf8(source)))) {
    return false;
  }
  memcpy(output, source, length + 1);
  return true;
}

template <typename T>
void golfAddJsonHoleArray(JsonObject parent, const char* name, const T* values, const uint8_t count,
                          const bool writeZeros = false) {
  JsonArray array = parent[name].to<JsonArray>();
  for (uint8_t hole = 0; hole < count; ++hole) array.add(writeZeros ? 0 : values[hole]);
}

template <typename T>
void golfAddJsonHoleArray(JsonDocument& doc, const char* name, const T* values, const uint8_t count,
                          const bool writeZeros = false) {
  JsonArray array = doc[name].to<JsonArray>();
  for (uint8_t hole = 0; hole < count; ++hole) array.add(writeZeros ? 0 : values[hole]);
}

inline void golfAddJsonPenalties(JsonObject parent, const GolfPlayerScore& score, const uint8_t holeCount,
                                 const bool writeZeros = false) {
  JsonArray holes = parent["penalties"].to<JsonArray>();
  for (uint8_t hole = 0; hole < holeCount; ++hole) {
    JsonArray events = holes.add<JsonArray>();
    if (writeZeros) continue;
    const uint8_t count = score.penaltyCount[hole] < GolfRound::MAX_PENALTIES_PER_HOLE
                              ? score.penaltyCount[hole]
                              : GolfRound::MAX_PENALTIES_PER_HOLE;
    for (uint8_t index = 0; index < count; ++index) {
      GolfPenaltyEvent event{};
      if (!golfPenaltyEventAt(score, hole, index, event)) continue;
      JsonArray pair = events.add<JsonArray>();
      pair.add(static_cast<uint8_t>(event.field));
      pair.add(static_cast<uint8_t>(event.kind));
    }
  }
}

inline void golfAddJsonPlayers(JsonDocument& doc, const GolfRound& round) {
  JsonArray players = doc["players"].to<JsonArray>();
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    const GolfPlayer& player = round.players[slot];
    const bool disabled = !golfPlayerIsEnabled(player);
    JsonObject encoded = players.add<JsonObject>();
    encoded["name"] = player.name;
    encoded["tee"] = golfTeeSelectionToken(player.tee);
    golfAddJsonHoleArray(encoded, "yards", player.yards, round.holeCount, disabled);
    golfAddJsonHoleArray(encoded, "putts", player.score.putts, round.holeCount, disabled);
    golfAddJsonHoleArray(encoded, "in100", player.score.in100, round.holeCount, disabled);
    golfAddJsonHoleArray(encoded, "out100", player.score.out100, round.holeCount, disabled);
    golfAddJsonPenalties(encoded, player.score, round.holeCount, disabled);
  }
}

inline bool golfReadJsonPenalties(const JsonVariantConst value, GolfPlayerScore& score, uint16_t& holeCount) {
  const JsonArrayConst holes = value.as<JsonArrayConst>();
  if (holes.isNull()) {
    holeCount = 0;
    return false;
  }
  const size_t size = holes.size();
  holeCount = size > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(size);
  const uint16_t readHoles = holeCount < GolfRound::MAX_HOLES ? holeCount : GolfRound::MAX_HOLES;
  for (uint16_t hole = 0; hole < readHoles; ++hole) {
    const JsonArrayConst events = holes[hole].as<JsonArrayConst>();
    if (events.isNull()) {
      score.penaltyCount[hole] = 1;
      score.penaltyEvents[hole][0] = 0x08;
      continue;
    }
    const size_t eventCount = events.size();
    score.penaltyCount[hole] = eventCount > GolfRound::MAX_PENALTIES_PER_HOLE
                                   ? static_cast<uint8_t>(GolfRound::MAX_PENALTIES_PER_HOLE + 1)
                                   : static_cast<uint8_t>(eventCount);
    const uint8_t readEvents = eventCount < GolfRound::MAX_PENALTIES_PER_HOLE ? static_cast<uint8_t>(eventCount)
                                                                              : GolfRound::MAX_PENALTIES_PER_HOLE;
    for (uint8_t index = 0; index < readEvents; ++index) {
      uint8_t packed = 0x08;
      const JsonArrayConst pair = events[index].as<JsonArrayConst>();
      if (!pair.isNull() && pair.size() == 2 && pair[0].is<int>() && pair[1].is<int>()) {
        const int field = pair[0].as<int>();
        const int kind = pair[1].as<int>();
        if (field >= 0 && field <= static_cast<int>(GolfField::Out100) && kind >= 0 &&
            kind <= static_cast<int>(GolfPenaltyKind::Ob)) {
          packed = golfPackPenaltyEvent(static_cast<GolfField>(field), static_cast<GolfPenaltyKind>(kind));
        }
      }
      const uint8_t shift = index % 2 == 0 ? 0 : 4;
      uint8_t& target = score.penaltyEvents[hole][index / 2];
      target = static_cast<uint8_t>((target & ~(0x0f << shift)) | ((packed & 0x0f) << shift));
    }
  }
  return true;
}

template <typename T>
bool golfReadJsonHoleArray(const JsonVariantConst value, T* output, const uint16_t capacity, const int32_t maximum,
                           uint16_t& count) {
  const JsonArrayConst array = value.as<JsonArrayConst>();
  if (array.isNull()) {
    count = 0;
    return false;
  }
  const size_t size = array.size();
  count = size > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(size);
  const uint16_t readCount = count < capacity ? count : capacity;
  for (uint16_t index = 0; index < readCount; ++index) {
    const JsonVariantConst element = array[index];
    if (!element.is<int>()) return false;
    const int item = element.as<int>();
    if (item < 0 || item > maximum) return false;
    output[index] = static_cast<T>(item);
  }
  return true;
}

inline bool golfReadJsonPlayer(const JsonVariantConst value, GolfPlayer& player, GolfPlayerColumnLengths& lengths) {
  const JsonObjectConst object = value.as<JsonObjectConst>();
  if (object.isNull()) return false;
  const char* tee = object["tee"].is<const char*>() ? object["tee"].as<const char*>() : nullptr;
  if (!golfReadJsonString(object["name"], player.name, sizeof(player.name), true) ||
      player.name[0] == '\0' || !golfParseTeeSelection(tee, player.tee)) {
    return false;
  }
  return golfReadJsonHoleArray(object["yards"], player.yards, GolfRound::MAX_HOLES, UINT16_MAX, lengths.yards) &&
         golfReadJsonHoleArray(object["putts"], player.score.putts, GolfRound::MAX_HOLES, 99, lengths.putts) &&
         golfReadJsonHoleArray(object["in100"], player.score.in100, GolfRound::MAX_HOLES, 99, lengths.in100) &&
         golfReadJsonHoleArray(object["out100"], player.score.out100, GolfRound::MAX_HOLES, 99, lengths.out100) &&
         golfReadJsonPenalties(object["penalties"], player.score, lengths.penalties);
}

inline GolfRoundDecodeStatus golfDecodeRoundJson(const JsonVariantConst doc, const bool stateFile, GolfRound& out,
                                                 GolfValidationResult& validation) {
  const int version = doc["v"] | 0;
  if (version != 2 && version != 3 && version != 4) return GolfRoundDecodeStatus::RejectedVersion;

  const int holes = doc["holes"] | 0;
  const int currentHole = stateFile ? (doc["currentHole"] | -1) : 0;
  int currentPlayer = version == 4 && stateFile ? (doc["currentPlayer"] | -1) : 0;
  bool validDate = doc["date"].isNull();
  out = {};
  initializeGolfPlayerDefaults(out);
  if (!validDate && doc["date"].is<const char*>()) {
    validDate = golfParseDate(doc["date"].as<const char*>(), out.dateYmd);
  }
  if (holes < 0 || holes > UINT8_MAX || currentHole < 0 || (version == 4 && stateFile && currentPlayer < 0) ||
      !validDate || !golfReadJsonString(doc["course"], out.courseName, sizeof(out.courseName), true)) {
    return GolfRoundDecodeStatus::RejectedMetadata;
  }

  GolfRoundColumnLengths lengths{};
  if (!golfReadJsonHoleArray(doc["par"], out.par, GolfRound::MAX_HOLES, UINT8_MAX, lengths.par)) {
    return GolfRoundDecodeStatus::RejectedArrayLength;
  }

  if (version == 4) {
    if (!doc["hasSi"].is<bool>()) return GolfRoundDecodeStatus::RejectedMetadata;
    out.hasSi = doc["hasSi"].as<bool>();
    if (!golfReadJsonHoleArray(doc["si"], out.si, GolfRound::MAX_HOLES, UINT8_MAX, lengths.si)) {
      return GolfRoundDecodeStatus::RejectedArrayLength;
    }
    const JsonArrayConst players = doc["players"].as<JsonArrayConst>();
    if (players.isNull()) return GolfRoundDecodeStatus::RejectedPlayerCount;
    const size_t playerCount = players.size();
    lengths.players = playerCount > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(playerCount);
    const uint8_t readPlayers = playerCount < GolfRound::MAX_PLAYERS ? static_cast<uint8_t>(playerCount)
                                                                     : GolfRound::MAX_PLAYERS;
    for (uint8_t slot = 0; slot < readPlayers; ++slot) {
      if (!golfReadJsonPlayer(players[slot], out.players[slot], lengths.player[slot])) {
        return GolfRoundDecodeStatus::RejectedMetadata;
      }
    }
    if (!stateFile) {
      currentPlayer = GolfRound::NO_PLAYER;
      for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
        if (golfPlayerIsEnabled(out.players[slot])) {
          currentPlayer = slot;
          break;
        }
      }
    }
  } else {
    const char* legacyTee = doc["tees"].is<const char*>() ? doc["tees"].as<const char*>() : nullptr;
    out.players[0].tee = golfLegacyTeeSelection(legacyTee);
    out.currentPlayer = 0;
    GolfPlayer& player = out.players[0];
    lengths.expectLegacyYards = stateFile;
    if ((stateFile && !golfReadJsonHoleArray(doc["yards"], player.yards, GolfRound::MAX_HOLES, UINT16_MAX,
                                             lengths.player[0].yards)) ||
        !golfReadJsonHoleArray(doc["putts"], player.score.putts, GolfRound::MAX_HOLES, 99,
                              lengths.player[0].putts) ||
        !golfReadJsonHoleArray(doc["in100"], player.score.in100, GolfRound::MAX_HOLES, 99,
                              lengths.player[0].in100) ||
        !golfReadJsonHoleArray(doc["out100"], player.score.out100, GolfRound::MAX_HOLES, 99,
                              lengths.player[0].out100) ||
        (version == 3 &&
         !golfReadJsonPenalties(doc["penalties"], player.score, lengths.player[0].penalties))) {
      return GolfRoundDecodeStatus::RejectedArrayLength;
    }
  }

  return golfCheckRound(out, version, holes, currentHole, currentPlayer, lengths, validation);
}

#endif
