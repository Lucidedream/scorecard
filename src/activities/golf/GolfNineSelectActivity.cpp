#include "GolfNineSelectActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>
#include <Logging.h>

#include <cstdio>

#include "GolfNavigation.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "golf/GolfNinePicker.h"

namespace fui = freeink::ui;

GolfNineSelectActivity::GolfNineSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               const GolfCourseFile& file, GolfNineSelection initial)
    : GolfUiListActivity("GolfNineSelect", renderer, mappedInput), courseFile(file), selection(initial) {}

void GolfNineSelectActivity::onEnter() {
  phase = Phase::Rows;
  loadFailed =
      !CourseStore::load(courseFile, course) || course.nineCount < 2 || !golfNineSelectionValid(course, selection);
  if (loadFailed) {
    LOG_ERR("GOLF", "Nine picker: course load or selection failed");
  } else {
    for (uint8_t nine = 0; nine < course.nineCount; ++nine) {
      const unsigned par = golfNinePar(course, nine);
      snprintf(parLabels[nine], sizeof(parLabels[nine]), "%u", par);
      uint32_t yards = 0;
      if (golfNineLongestTeeYards(course, nine, yards)) {
        char yardLabel[12]{};
        if (yards >= 1000) {
          snprintf(yardLabel, sizeof(yardLabel), "%u,%03u", static_cast<unsigned>(yards / 1000),
                   static_cast<unsigned>(yards % 1000));
        } else {
          snprintf(yardLabel, sizeof(yardLabel), "%u", static_cast<unsigned>(yards));
        }
        snprintf(details[nine], sizeof(details[nine]), tr(STR_GOLF_NINE_DETAIL_FORMAT), par, yardLabel);
      } else {
        snprintf(details[nine], sizeof(details[nine]), tr(STR_GOLF_NINE_PAR_FORMAT), par);
      }
    }
    refreshRows();
  }
  UiListActivity::onEnter();
}

int GolfNineSelectActivity::listCount() const {
  if (loadFailed) return 0;
  return phase == Phase::Rows ? ROW_COUNT : course.nineCount;
}

const char* GolfNineSelectActivity::headerTitle() const {
  return loadFailed ? tr(STR_GOLF_COURSE_LOAD_ERROR) : course.courseName;
}

void GolfNineSelectActivity::refreshRows() {
  for (auto& row : rows) row = {};
  if (phase == Phase::NineList) {
    for (uint8_t nine = 0; nine < course.nineCount; ++nine) {
      rows[nine].label = course.nines[nine].name;
      rows[nine].value = parLabels[nine];
      rows[nine].subtitle = details[nine];
      rows[nine].actionValue = nine;
      rows[nine].enabled = true;
    }
    return;
  }
  rows[0].label = tr(STR_GOLF_FRONT_NINE);
  rows[0].value = course.nines[selection.front].name;
  rows[0].subtitle = details[selection.front];
  rows[1].label = tr(STR_GOLF_BACK_NINE);
  rows[1].value = course.nines[selection.back].name;
  rows[1].subtitle = details[selection.back];
  rows[2].label = tr(STR_GOLF_SWAP_NINES);
  rows[3].label = tr(STR_GOLF_CONTINUE);
  for (uint8_t row = 0; row < ROW_COUNT; ++row) {
    rows[row].actionValue = row;
    rows[row].enabled = true;
  }
}

void GolfNineSelectActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index < 0 || index >= listCount() || !rows[index].enabled) return;
  if (phase == Phase::Rows && index == 3) {
    openGolfPlayerSetup(activityManager, renderer, mappedInput, courseFile, selection);
    return;
  }
  {
    RenderLock lock(*this);
    if (phase == Phase::NineList) {
      golfChooseNine(course, selection, backSlot, static_cast<uint8_t>(index));
      phase = Phase::Rows;
      nav.reset(backSlot ? 1 : 0);
    } else if (index == 2) {
      golfSwapNines(course, selection);
      nav.reset(2);
    } else {
      backSlot = index == 1;
      phase = Phase::NineList;
      nav.reset(backSlot ? selection.back : selection.front);
    }
    refreshRows();
    closeRouting();
  }
  requestUpdate();
}

void GolfNineSelectActivity::returnToRows() {
  {
    RenderLock lock(*this);
    phase = Phase::Rows;
    refreshRows();
    closeRouting();
    nav.reset(backSlot ? 1 : 0);
  }
  requestUpdate();
}

void GolfNineSelectActivity::onBackButton() {
  if (!loadFailed && phase == Phase::NineList) {
    returnToRows();
  } else {
    openGolfSetup(activityManager, renderer, mappedInput);
  }
}

void GolfNineSelectActivity::navigateButtons() {
  if (!loadFailed) UiListActivity::navigateButtons();
}

void GolfNineSelectActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, mappedInput, metrics.topPadding);
  const char* subtitle = nullptr;
  if (!loadFailed) {
    subtitle = phase == Phase::Rows ? tr(STR_GOLF_CHOOSE_NINES)
                                    : (backSlot ? tr(STR_GOLF_BACK_NINE) : tr(STR_GOLF_FRONT_NINE));
  }
  golfui::drawHeader(renderer, layout.header, headerTitle(), subtitle);
}

golfui::FooterLabels GolfNineSelectActivity::golfFooterLabels() const {
  if (loadFailed) return {tr(STR_BACK), "", "", ""};
  return GolfUiListActivity::golfFooterLabels();
}

void GolfNineSelectActivity::buildScreen(UiScreen& screen) {
  if (loadFailed) return;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, mappedInput, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  listProps = {};
  listProps.items = rows;
  listProps.count = static_cast<uint16_t>(listCount());
  listProps.action = ACTION_ROW;
  listProps.inputMask = fui::InputTouch;
  listProps.valueInset = 8;
  syncListViewport(screen, listProps, true);
  screen.list(listProps);
}

#endif
