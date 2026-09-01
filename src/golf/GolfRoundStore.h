#pragma once

#include <PersistableStore.h>

#include "GolfArchiveMarker.h"
#include "GolfRound.h"

class GolfRoundStore : public PersistableStore<GolfRoundStore> {
  GolfRoundStore() { initializeGolfPlayerDefaults(round); }

  friend class PersistableStore<GolfRoundStore>;

 public:
  static const char* getFilePath() { return "/golf/state.json"; }

  GolfRound& getRound() { return round; }
  const GolfRound& getRound() const { return round; }
  void setRound(const GolfRound& value) {
    round = value;
    clearGolfArchiveMarker(archiveMarker);
  }

  bool isArchived() const { return isGolfArchiveMarked(archiveMarker); }
  const char* archivedFilename() const { return golfArchivedFilename(archiveMarker); }
  bool markArchivedAs(const char* filename);

  bool saveToFile() const;
  bool loadFromFile();
  bool clear();

  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

 private:
  GolfRound round{};
  GolfArchiveMarker archiveMarker{};
};

#define GOLF_ROUND_STORE GolfRoundStore::getInstance()
