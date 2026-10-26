#pragma once
#if defined(CROSSPOINT_GOLF)
#include <HalStorage.h>
#include <Memory.h>

#include "GolfNavigation.h"
#include "GolfUiListActivity.h"
#include "golf/CourseStore.h"
#include "golf/GolfCatalog.h"
#include "golf/GolfCourseCatalogInfo.h"

class GolfCoursePickerActivity final : public GolfUiListActivity {
 public:
  using Mode = GolfPickerMode;
  GolfCoursePickerActivity(GfxRenderer& renderer, MappedInputManager& input, Mode mode = Mode::Browse)
      : GolfUiListActivity("GolfCoursePicker", renderer, input), mode(mode) {}
  GolfCoursePickerActivity(GfxRenderer& renderer, MappedInputManager& input, GolfPickerContext context)
      : GolfUiListActivity("GolfCoursePicker", renderer, input),
        mode(context.mode),
        initial(context),
        standalone(true) {}
  void restoreInfo(const GolfCourseFile& file, uint16_t row) {
    infoFile = file;
    infoRow = row;
    reopenInfo = true;
  }
  void onEnter() override;
  void onExit() override;

 private:
  using Level = GolfPickerLevel;
  Mode mode;
  GolfPickerContext initial{};
  GolfCourseFile infoFile{};
  uint16_t infoRow = 0;
  bool reopenInfo = false;
  bool standalone = false;
  GolfPickerContext context(uint16_t selected) const;
  static constexpr uint8_t PAGE_ROWS = 16;
  struct Scratch {
    GolfCatalogRegionsReader catalog;
    GolfCourse course;
    GolfCatalogRecord a;
    GolfCourseCatalogInfo info;
    char bytes[GOLF_CATALOG_RECORD_SIZE];
    char chunk[128];
  };
  std::unique_ptr<Scratch> scratch;
  GolfCourseFile recent[5]{};
  uint8_t recentCount = 0;
  uint8_t ownerCount = 0;
  char hubDetail[256]{};
  uint16_t mapCount = 0;
  char figures[PAGE_ROWS][16]{};
  char units[PAGE_ROWS][32]{};
  const char* hubLabel(int index) const;
  bool handleCustomInput() override;
  void navigateButtons() override;
  golfui::FooterLabels golfFooterLabels() const override;

  uint16_t records[GOLF_CATALOG_MAX_REGION_COURSES]{};
  uint8_t ratingKeys[GOLF_CATALOG_MAX_REGION_COURSES]{};
  uint16_t recordCount = 0;
  uint16_t recordReads = 0;
  uint8_t region = 0;
  bool hasCatalog = false;
  bool byRating = true;
  Level level = Level::Hub;
  freeink::ui::ListItem rows[PAGE_ROWS]{};
  char names[PAGE_ROWS][40]{};
  char details[PAGE_ROWS][96]{};
  freeink::ui::ListProps props{};
  HalFile indexFile;
  int listCount() const override;
  const char* headerTitle() const override;
  void buildScreen(UiScreen& screen) override;
  void drawChrome() override;
  void activateIndex(int index) override;
  void onBackButton() override;
  void enterLevel(Level next);
  void openMyCourses(bool finishOnReturn = false);
  void openInfo(const GolfCourseFile& file);
  bool loadCatalog();
  void loadRecent();
  bool readRecord(uint16_t ordinal, GolfCatalogRecord& record);
  void loadRegion();
  void openSearch();
  void loadSearch();
  void sortSearch();
  uint8_t regionPageSize = 10;
  std::unique_ptr<GolfCatalogSearchKey[]> searchKeys;
  bool searchCapped = false;
  bool isCourseLevel() const { return level == Level::Courses || level == Level::Search; }
  bool hasSort() const { return level == Level::Courses || (level == Level::Search && recordCount && searchKeys); }
  void sortRegion();
  void fillRow(uint8_t slot, uint16_t absoluteIndex);
  static bool resolveRecent(const char* name, void* user);
};
#endif
