#pragma once
#if defined(CROSSPOINT_GOLF)
#include <HalStorage.h>
#include <Memory.h>

#include "GolfUiListActivity.h"
#include "golf/CourseStore.h"
#include "golf/GolfCatalog.h"

class GolfCoursePickerActivity final : public GolfUiListActivity {
 public:
  enum class Mode : uint8_t { Browse };
  GolfCoursePickerActivity(GfxRenderer& renderer, MappedInputManager& input, Mode = Mode::Browse)
      : GolfUiListActivity("GolfCoursePicker", renderer, input) {}
  void onEnter() override;
  void onExit() override;

 private:
  enum class Level : uint8_t { Hub, Regions, Courses };
  static constexpr uint8_t PAGE_ROWS = 16;
  struct Scratch {
    GolfCatalogRegionsReader catalog;
    GolfCourse course;
    GolfCatalogRecord a;
    char bytes[GOLF_CATALOG_RECORD_SIZE];
    char chunk[128];
  };
  std::unique_ptr<Scratch> scratch;
  GolfCourseFile recent[5]{};
  uint8_t recentCount = 0;
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
  void sortRegion();
  void fillRow(uint8_t slot, uint16_t absoluteIndex);
  static bool resolveRecent(const char* name, void* user);
};
#endif
