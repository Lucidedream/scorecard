#pragma once

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfHistory.h"
#include "golf/GolfIndexMigrate.h"
#include "golf/GolfQuotes.h"

class GolfHomeActivity final : public Activity, protected UiAppHost {
 public:
  GolfHomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GolfHome", renderer, mappedInput), UiAppHost(renderer) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Destination : uint8_t { NewRound, History, CourseMap, Tips };
  static constexpr freeink::ui::ActionId ACTION_TILE = 1;
  static constexpr freeink::ui::ActionId ACTION_RESUME = 2;

  GolfPlayerNamesReader indexSummary{};
  GolfIndexMigrator recovery{};
  char chunk[128]{};
  char detailLine[96]{};
  char quoteText[GOLF_QUOTE_TEXT_CAPACITY]{};
  char quoteAuthor[GOLF_QUOTE_AUTHOR_CAPACITY]{};
  Destination destinations[4]{};
  uint8_t destinationCount = 0;
  uint8_t selected = 0;
  bool resumeFocused = false;
  bool hasOpenRound = false;
  bool showNewRound = true;
  bool stateError = false;
  bool cleanupError = false;
  bool indexLoadError = false;
  uint8_t tipsNoteCount = 0;
  bool tipsError = false;
  uint8_t courseCount = 0;
  bool courseCountError = false;
  bool quotePresent = false;
  bool quoteHasAuthor = false;

  static void screenTrampoline(UiScreen& screen, void* user);
  static void actionTrampoline(const freeink::ui::ActionEvent& event, void* user);
  void buildScreen(UiScreen& screen);
  void scanIndexSummary();
  void scanCourseCount();
  void refreshDetail();
  void moveSelection(int delta);
  void activateSelected();
  void activateDestination(Destination destination);
  const char* destinationLabel(Destination destination) const;
  freeink::ui::BitmapRef destinationIcon(Destination destination) const;
  const char* headerTitle() const;
  void drawFooter() const;
};
