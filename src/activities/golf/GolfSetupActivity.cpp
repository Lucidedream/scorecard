#include "GolfSetupActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>

#include <cctype>
#include <cstdio>
#include <cstring>

#include "GolfNavigation.h"
#include "GolfStrings.h"
#include "components/UITheme.h"
#include "golf/GolfRoundStore.h"
#include "golf/RoundArchive.h"

namespace fui = freeink::ui;

namespace {

int compareCourseNames(const char* left, const char* right) {
  while (*left != '\0' && *right != '\0') {
    const int a = std::tolower(static_cast<unsigned char>(*left));
    const int b = std::tolower(static_cast<unsigned char>(*right));
    if (a != b) return a - b;
    ++left;
    ++right;
  }
  return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

bool leapYear(const uint16_t year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

uint8_t daysInMonth(const uint16_t year, const uint8_t month) {
  static constexpr uint8_t DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && leapYear(year) ? 29 : DAYS[month - 1];
}

}  // namespace

void GolfSetupActivity::onEnter() {
  phase = Phase::Course;
  loadCourses();
  uint16_t lastDate = 0;
  if (RoundArchive::lastRoundDate(lastDate)) dateYmd = lastDate;
  UiListActivity::onEnter();
}

void GolfSetupActivity::loadCourses() {
  const GolfCourseListResult result = CourseStore::enumerate(files, GOLF_MAX_COURSES);
  overflow = result.overflow;
  courseCount = 0;
  for (uint8_t i = 0; i < result.count; ++i) {
    GolfCourse course{};
    if (CourseStore::load(files[i].filename, course)) courses[courseCount++] = course;
  }
  for (uint8_t i = 1; i < courseCount; ++i) {
    const GolfCourse value = courses[i];
    uint8_t position = i;
    while (position > 0 && compareCourseNames(value.courseName, courses[position - 1].courseName) < 0) {
      courses[position] = courses[position - 1];
      --position;
    }
    courses[position] = value;
  }
  noCourses = courseCount == 0;
  uint8_t row = 0;
  for (; row < courseCount; ++row) {
    rows[row] = {};
    rows[row].label = courses[row].courseName;
    rows[row].subtitle = courses[row].tees[0] == '\0' ? nullptr : courses[row].tees;
    rows[row].actionValue = row;
  }
  if (overflow) {
    rows[row] = {};
    rows[row].label = GolfStrings::COURSE_OVERFLOW;
    rows[row].enabled = false;
    ++row;
  }
  rows[row] = {};
  rows[row].label = GolfStrings::QUICK_ROUND;
  rows[row].actionValue = row;
}

int GolfSetupActivity::listCount() const {
  if (phase == Phase::Date) return 4;
  return courseCount + (overflow ? 1 : 0) + 1;
}

const char* GolfSetupActivity::headerTitle() const {
  if (saveFailed) return GolfStrings::SAVE_ERROR;
  if (phase == Phase::Date) return GolfStrings::DATE;
  return noCourses ? GolfStrings::NO_COURSES : GolfStrings::COURSES;
}

void GolfSetupActivity::onBackButton() {
  if (phase == Phase::Date) {
    phase = Phase::Course;
    nav.reset();
    requestUpdate();
    return;
  }
  openGolfHome(activityManager, renderer, mappedInput);
}

void GolfSetupActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (phase == Phase::Date) {
    if (index == 3) startRound();
    return;
  }
  if (index < courseCount) {
    selectCourse(courses[index]);
    return;
  }
  const int quickIndex = courseCount + (overflow ? 1 : 0);
  if (index != quickIndex) return;
  GolfCourse quick{};
  memcpy(quick.courseName, GolfStrings::QUICK_ROUND, sizeof(GolfStrings::QUICK_ROUND));
  quick.holeCount = GolfRound::MAX_HOLES;
  for (uint8_t hole = 0; hole < quick.holeCount; ++hole) quick.par[hole] = 4;
  selectCourse(quick);
}

void GolfSetupActivity::selectCourse(const GolfCourse& course) {
  chosenCourse = course;
  phase = Phase::Date;
  saveFailed = false;
  nav.reset();
  buildDateRows();
  requestUpdate();
}

void GolfSetupActivity::buildDateRows() {
  const uint16_t year = 2000 + (dateYmd >> 9);
  const uint8_t month = (dateYmd >> 5) & 0x0F;
  const uint8_t day = dateYmd & 0x1F;
  snprintf(dateValues[0], sizeof(dateValues[0]), "%u", year);
  snprintf(dateValues[1], sizeof(dateValues[1]), "%u", month);
  snprintf(dateValues[2], sizeof(dateValues[2]), "%u", day);
  const char* labels[] = {GolfStrings::YEAR, GolfStrings::MONTH, GolfStrings::DAY};
  for (uint8_t i = 0; i < 3; ++i) {
    rows[i] = {};
    rows[i].label = labels[i];
    rows[i].value = dateValues[i];
    rows[i].actionValue = i;
  }
  rows[3] = {};
  rows[3].label = GolfStrings::START_ROUND;
  rows[3].actionValue = 3;
}

void GolfSetupActivity::adjustDate(const int delta) {
  uint16_t year = 2000 + (dateYmd >> 9);
  uint8_t month = (dateYmd >> 5) & 0x0F;
  uint8_t day = dateYmd & 0x1F;
  if (nav.selected == 0) {
    year = static_cast<uint16_t>(year + delta);
    if (year < 2000) year = 2127;
    if (year > 2127) year = 2000;
  } else if (nav.selected == 1) {
    int value = static_cast<int>(month) + delta;
    if (value < 1) value = 12;
    if (value > 12) value = 1;
    month = static_cast<uint8_t>(value);
  } else if (nav.selected == 2) {
    const uint8_t maximum = daysInMonth(year, month);
    int value = static_cast<int>(day) + delta;
    if (value < 1) value = maximum;
    if (value > maximum) value = 1;
    day = static_cast<uint8_t>(value);
  }
  const uint8_t maximum = daysInMonth(year, month);
  if (day > maximum) day = maximum;
  dateYmd = static_cast<uint16_t>(((year - 2000) << 9) | (month << 5) | day);
  buildDateRows();
  requestUpdate();
}

bool GolfSetupActivity::handleCustomInput() {
  if (phase != Phase::Date) return false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBackButton();
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    moveSelectionTo(ButtonNavigator::previousIndex(nav.selected, 4));
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (nav.selected == 3 && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      startRound();
    } else {
      moveSelectionTo(ButtonNavigator::nextIndex(nav.selected, 4));
    }
    return true;
  }
  if (nav.selected < 3 && mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    adjustDate(1);
    return true;
  }
  if (nav.selected < 3 && mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    adjustDate(-1);
    return true;
  }
  return true;
}

void GolfSetupActivity::startRound() {
  GolfRound round{};
  CourseStore::applyGolfCourse(chosenCourse, round, dateYmd);
  GOLF_ROUND_STORE.setRound(round);
  if (!GOLF_ROUND_STORE.saveToFile()) {
    saveFailed = true;
    requestUpdate();
    return;
  }
  openGolfScoring(activityManager, renderer, mappedInput);
}

void GolfSetupActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows;
  props.count = static_cast<uint16_t>(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.subtitleText = screen.theme().smallText;
  syncListViewport(screen, props, phase == Phase::Course);
  screen.list(props);
}

void GolfSetupActivity::drawFooter() {
  if (phase == Phase::Course) {
    UiListActivity::drawFooter();
    return;
  }
  const auto labels =
      mappedInput.mapDirectionalLabels(GolfStrings::BACK, GolfStrings::NEXT, GolfStrings::LEFT, GolfStrings::RIGHT,
                                       GolfStrings::SWITCH_FIELD, GolfStrings::SWITCH_FIELD);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

#endif
