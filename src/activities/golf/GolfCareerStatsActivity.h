#pragma once

#include <FreeInkUICore.h>

#include <memory>

#include "GolfReviewFormat.h"
#include "activities/Activity.h"
#include "golf/GolfCareerStats.h"
#include "golf/GolfHistory.h"
#include "golf/GolfIndexMigrate.h"
#include "golf/GolfPaths.h"
#include "golf/GolfRound.h"

// Career Stats (CONTRACTS-V2 §33.4): the third row on GolfHistoryChoiceActivity.
// On entry it streams every archived round for this player slot, folds each into
// a GolfCareerTally (GolfCareerStats.h), and renders five sections — hero,
// score shape, by par, around the green, records — as a scrolling list under a
// fixed golf header and a BACK / MORE footer.
class GolfCareerStatsActivity final : public Activity {
 public:
  GolfCareerStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint8_t playerSlot,
                          const char* playerName);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return scanning; }

 private:
  static constexpr int SIDE_PADDING = 16;
  static constexpr uint8_t MAX_BODY_ROWS = 24;

  struct RowSpec {
    bool isHeader = false;
    char label[24]{};
    char value[16]{};
    char right[24]{};
    bool hasBar = false;
    uint16_t barCount = 0;
    uint16_t barMax = 0;
  };

  // One checked allocation holds the index-streaming reader, the recovery
  // scratch, the per-round locator and the reused GolfRound. Never per round.
  struct ScanScratch {
    GolfHistoryReader history{};
    GolfIndexMigrator recovery{};
    GolfIndexFileLocator locator{};
    GolfRound round{};
    char chunk[128]{};
    char path[sizeof("/golf/rounds/") + GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  };

  const uint8_t playerSlot;
  char playerName[GolfPlayer::NAME_CAPACITY]{};
  char playerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};

  std::unique_ptr<ScanScratch> scratch;
  GolfCareerTally tally{};
  bool scanning = true;
  bool scanError = false;
  uint16_t roundsToRead = 0;
  bool firstPaint = true;

  RowSpec bodyRows[MAX_BODY_ROWS]{};
  uint8_t bodyRowCount = 0;
  int topItem = 0;
  int visibleCount = 1;
  bool overflow = false;

  void runScan();
  bool streamIndex();
  bool resolveRoundFile(uint8_t newestIndex);
  static void logMalformed(uint32_t lineNumber, void* user);

  void buildBodyRows();
  void page(int direction);
  void drawHeroBand(freeink::ui::Rect rect) const;
  void drawBodyRow(freeink::ui::Rect rect, const RowSpec& row) const;
  void drawFooter() const;
  void finishPaint();
};
