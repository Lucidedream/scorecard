#include "GolfHomeActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <FreeInkUIIcon.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <cstdio>

#include "GolfCourseMapListActivity.h"
#include "GolfNavigation.h"
#include "GolfPlayerSelectActivity.h"
#include "GolfSetupActivity.h"
#include "GolfTipListActivity.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "components/icons/golfTileIcons.h"
#include "golf/CourseStore.h"
#include "golf/GolfQuotesStore.h"
#include "golf/GolfRoundStore.h"
#include "golf/GolfTips.h"
#include "golf/RoundArchive.h"

namespace fui = freeink::ui;

namespace {

constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";

}

void GolfHomeActivity::onEnter() {
  // The RAM marker is the commit authority. Never read an older unmarked
  // state.json over it; a failed cleanup may only be retried.
  const bool archiveMarkedAtEntry = GOLF_ROUND_STORE.isArchived();
  bool archiveMarkerSeen = archiveMarkedAtEntry;
  bool cleanupSucceeded = true;
  bool stateExists = false;
  bool loaded = false;
  if (archiveMarkedAtEntry) {
    cleanupSucceeded = GOLF_ROUND_STORE.clear();
  } else {
    stateExists = Storage.exists(GolfRoundStore::getFilePath());
    loaded = stateExists && GOLF_ROUND_STORE.loadFromFile();
    archiveMarkerSeen = GOLF_ROUND_STORE.isArchived();
    if (archiveMarkerSeen) cleanupSucceeded = GOLF_ROUND_STORE.clear();
  }

  const GolfHomeEntryDecision decision = golfDecideHomeEntry(archiveMarkerSeen, cleanupSucceeded, stateExists, loaded,
                                                             loaded ? GOLF_ROUND_STORE.getRound().holeCount : 0);
  hasOpenRound = decision.showResume;
  showNewRound = decision.showNew;
  stateError = decision.stateError;
  cleanupError = decision.cleanupOnly;

  destinationCount = 0;
  if (showNewRound) destinations[destinationCount++] = Destination::NewRound;
  destinations[destinationCount++] = Destination::History;
  destinations[destinationCount++] = Destination::CourseMap;
  destinations[destinationCount++] = Destination::Tips;
  selected = 0;
  resumeFocused = hasOpenRound;
  scanIndexSummary();
  quotePresent = golfPickRandomQuote(quoteText, sizeof(quoteText), quoteAuthor, sizeof(quoteAuthor), quoteHasAuthor);
  {
    const GolfTipsListResult tips = GolfTipsStore::enumerate(nullptr, GOLF_MAX_TIPS);
    tipsError = tips.directoryError;
    tipsNoteCount = tips.count;
  }
  scanCourseCount();
  refreshDetail();

  Activity::onEnter();
  resetUi();
  app.on(ACTION_TILE, &GolfHomeActivity::actionTrampoline, this);
  app.on(ACTION_RESUME, &GolfHomeActivity::actionTrampoline, this);
  app.setScreen(&GolfHomeActivity::screenTrampoline, this);
  requestUpdate();
}

const char* GolfHomeActivity::headerTitle() const {
  if (cleanupError) return tr(STR_GOLF_ARCHIVE_CLEANUP_ERROR);
  return stateError ? tr(STR_GOLF_STATE_ERROR) : tr(STR_GOLF_APP_TITLE);
}

void GolfHomeActivity::scanIndexSummary() {
  indexSummary.reset();
  indexLoadError = !RoundArchive::recoverIndex(recovery);
  if (indexLoadError || !Storage.exists(INDEX_PATH)) return;

  HalFile file;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) {
    indexLoadError = true;
    return;
  }
  while (file.available() > 0) {
    const int bytesRead = file.read(chunk, sizeof(chunk));
    if (bytesRead <= 0) {
      indexLoadError = true;
      indexSummary.reset();
      return;
    }
    indexSummary.feed(chunk, static_cast<size_t>(bytesRead));
  }
  indexSummary.finish();
}

const char* GolfHomeActivity::destinationLabel(const Destination destination) const {
  switch (destination) {
    case Destination::NewRound:
      return tr(STR_GOLF_NEW_ROUND);
    case Destination::History:
      return tr(STR_GOLF_HISTORY);
    case Destination::CourseMap:
      return tr(STR_GOLF_COURSE_MAP);
    case Destination::Tips:
    default:
      return tr(STR_GOLF_TIPS);
  }
}

fui::BitmapRef GolfHomeActivity::destinationIcon(const Destination destination) const {
  switch (destination) {
    case Destination::NewRound:
      return fui::bitmapFromIcon(icon_land_plot_32);
    case Destination::History:
      return fui::bitmapFromIcon(icon_scroll_text_32);
    case Destination::CourseMap:
      return fui::bitmapFromIcon(icon_map_32);
    case Destination::Tips:
    default:
      return fui::bitmapFromIcon(icon_lightbulb_32);
  }
}

void GolfHomeActivity::refreshDetail() {
  detailLine[0] = '\0';
  if (selected >= destinationCount) return;
  if (indexLoadError) {
    snprintf(detailLine, sizeof(detailLine), "%s", tr(STR_GOLF_ROUNDS_UNAVAILABLE_SHORT));
    return;
  }

  switch (destinations[selected]) {
    case Destination::NewRound: {
      if (!indexSummary.hasLatestRound()) {
        snprintf(detailLine, sizeof(detailLine), "%s", tr(STR_GOLF_NO_ROUNDS_YET));
        return;
      }
      const GolfIndexRow& latest = indexSummary.latestRound();
      if (latest.par == 0) {
        snprintf(detailLine, sizeof(detailLine), tr(STR_GOLF_LAST_ROUND_PAR_FREE_FORMAT), latest.course,
                 latest.strokes);
        return;
      }
      char toPar[8]{};
      const int difference = static_cast<int>(latest.strokes) - latest.par;
      if (difference == 0) {
        snprintf(toPar, sizeof(toPar), "%s", tr(STR_GOLF_EVEN));
      } else {
        snprintf(toPar, sizeof(toPar),
                 difference > 0 ? tr(STR_GOLF_TO_PAR_POSITIVE_FORMAT) : tr(STR_GOLF_TO_PAR_NEGATIVE_FORMAT),
                 difference);
      }
      snprintf(detailLine, sizeof(detailLine), tr(STR_GOLF_LAST_ROUND_FORMAT), latest.course, latest.strokes, toPar);
      return;
    }
    case Destination::History:
      snprintf(detailLine, sizeof(detailLine), tr(STR_GOLF_ROUNDS_RECORDED_FORMAT),
               static_cast<unsigned long>(indexSummary.totalRounds()));
      return;
    case Destination::CourseMap:
      if (courseCountError) {
        snprintf(detailLine, sizeof(detailLine), "%s", tr(STR_GOLF_COURSES_UNAVAILABLE));
      } else if (courseCount == 1) {
        snprintf(detailLine, sizeof(detailLine), "%s", tr(STR_GOLF_COURSES_COUNT_ONE));
      } else {
        snprintf(detailLine, sizeof(detailLine), tr(STR_GOLF_COURSES_COUNT_FORMAT), static_cast<unsigned>(courseCount));
      }
      return;
    case Destination::Tips:
      if (tipsError) {
        snprintf(detailLine, sizeof(detailLine), "%s", tr(STR_GOLF_TIPS_UNAVAILABLE));
      } else if (tipsNoteCount == 0) {
        snprintf(detailLine, sizeof(detailLine), "%s", tr(STR_GOLF_TIPS_NONE));
      } else if (tipsNoteCount == 1) {
        snprintf(detailLine, sizeof(detailLine), "%s", tr(STR_GOLF_TIPS_COUNT_ONE));
      } else {
        snprintf(detailLine, sizeof(detailLine), tr(STR_GOLF_TIPS_COUNT_FORMAT),
                 static_cast<unsigned long>(tipsNoteCount));
      }
      return;
  }
}

void GolfHomeActivity::scanCourseCount() {
  auto files = makeUniqueNoThrow<GolfCourseFile[]>(GOLF_MAX_COURSES);
  if (!files) {
    LOG_ERR("GOLF", "OOM: home course count scratch (%u entries)", GOLF_MAX_COURSES);
    courseCountError = true;
    courseCount = 0;
    return;
  }
  const GolfCourseListResult result = CourseStore::enumerate(files.get(), GOLF_MAX_COURSES);
  courseCountError = result.count == 0;
  courseCount = result.count;
}

void GolfHomeActivity::activateDestination(const Destination destination) {
  app.clearTapFlash();
  switch (destination) {
    case Destination::NewRound:
      openGolfSetup(activityManager, renderer, mappedInput);
      return;
    case Destination::Tips: {
      auto list = makeUniqueNoThrow<GolfTipListActivity>(renderer, mappedInput);
      if (!list) {
        LOG_ERR("GOLF", "OOM: tip list activity");
        return;
      }
      startActivityForResult(std::move(list), nullptr);
      return;
    }
    case Destination::CourseMap: {
      auto list = makeUniqueNoThrow<GolfCourseMapListActivity>(renderer, mappedInput);
      if (!list) {
        LOG_ERR("GOLF", "OOM: course map list activity");
        return;
      }
      startActivityForResult(std::move(list), nullptr);
      return;
    }
    case Destination::History:
      break;
  }
  auto selector = makeUniqueNoThrow<GolfPlayerSelectActivity>(renderer, mappedInput);
  if (!selector) {
    LOG_ERR("GOLF", "OOM: player selector activity");
    return;
  }
  startActivityForResult(std::move(selector), nullptr);
}

void GolfHomeActivity::activateSelected() {
  if (resumeFocused) {
    app.clearTapFlash();
    openGolfScoring(activityManager, renderer, mappedInput);
  } else if (selected < destinationCount) {
    activateDestination(destinations[selected]);
  }
}

void GolfHomeActivity::moveSelection(const int delta) {
  {
    RenderLock lock(*this);
    const uint8_t focusCount = static_cast<uint8_t>(destinationCount + (hasOpenRound ? 1 : 0));
    if (focusCount < 2) return;
    const uint8_t current = resumeFocused ? 0 : static_cast<uint8_t>(selected + (hasOpenRound ? 1 : 0));
    const uint8_t next = static_cast<uint8_t>((current + focusCount + delta) % focusCount);
    resumeFocused = hasOpenRound && next == 0;
    if (!resumeFocused) selected = static_cast<uint8_t>(next - (hasOpenRound ? 1 : 0));
    refreshDetail();
  }
  requestUpdate();
}

void GolfHomeActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelected();
    return;
  }
  const bool swapped = mappedInput.isNavDirectionSwapped();
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    moveSelection(golfFrontNavDelta(swapped, true));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    moveSelection(golfFrontNavDelta(swapped, false));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    moveSelection(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    moveSelection(1);
    return;
  }
  const auto route = routeTouch(mappedInput);
  if (route.routed && app.invalidated()) requestUpdate();
}

void GolfHomeActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<GolfHomeActivity*>(user)->buildScreen(screen);
}

void GolfHomeActivity::actionTrampoline(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<GolfHomeActivity*>(user);
  if (event.action == ACTION_RESUME) {
    self->app.clearTapFlash();
    openGolfScoring(activityManager, self->renderer, self->mappedInput);
    return;
  }
  if (event.action != ACTION_TILE || event.value < 0 || event.value >= self->destinationCount) return;
  {
    RenderLock lock(*self);
    self->resumeFocused = false;
    self->selected = static_cast<uint8_t>(event.value);
    self->refreshDetail();
  }
  self->activateSelected();
}

void GolfHomeActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  // Four tiles across 480 px leave ~118 px each; the row is a little shorter
  // than the three-tile layout so the 32 px icon and its label stay in
  // proportion at the narrower width (CONTRACTS-V2 §25.3).
  const fui::Rect tiles = screen.takeTop(112, static_cast<int16_t>(metrics.verticalSpacing));
  constexpr int16_t tileIconSize = 32;
  for (uint8_t index = 0; index < destinationCount; ++index) {
    const int left = tiles.x + static_cast<int32_t>(tiles.width) * index / destinationCount;
    const int right = tiles.x + static_cast<int32_t>(tiles.width) * (index + 1) / destinationCount;
    const fui::Rect tileRect{static_cast<int16_t>(left), tiles.y, static_cast<int16_t>(right - left), tiles.height};
    const bool tileSelected = !resumeFocused && index == selected;

    fui::ButtonProps tile{};
    tile.action = ACTION_TILE;
    tile.value = index;
    tile.inputMask = fui::InputTouch;
    tile.state = tileSelected ? fui::StateSelected : fui::StateNormal;
    screen.button(tile, tileRect);  // box, selection fill and hit target only

    const fui::Color ink = tileSelected ? fui::Color::White : fui::Color::Black;
    const fui::BitmapRef icon = destinationIcon(destinations[index]);
    const int16_t iconTop = static_cast<int16_t>(tileRect.y + 16);
    if (icon) {
      screen.target().bitmap(fui::Rect{static_cast<int16_t>(tileRect.x + (tileRect.width - tileIconSize) / 2), iconTop,
                                       tileIconSize, tileIconSize},
                             icon, fui::BitmapMode::Center, fui::Paint::solid(ink));
    }
    fui::TextStyle tileLabel = screen.theme().bodyText;
    tileLabel.bold = true;
    tileLabel.align = fui::TextAlign::Center;
    tileLabel.color = ink;
    tileLabel.maxLines = 2;
    const int16_t labelTop = static_cast<int16_t>(iconTop + tileIconSize + 6);
    screen.target().text(
        fui::Rect{tileRect.x, labelTop, tileRect.width, static_cast<int16_t>(tileRect.bottom() - labelTop - 4)},
        destinationLabel(destinations[index]), tileLabel);
  }

  if (hasOpenRound) {
    fui::ButtonProps resume{};
    resume.label = tr(STR_GOLF_RESUME_ROUND);
    resume.action = ACTION_RESUME;
    resume.inputMask = fui::InputTouch;
    resume.text = screen.theme().bodyText;
    resume.text.bold = true;
    resume.state = resumeFocused ? fui::StateSelected : fui::StateNormal;
    screen.button(resume, screen.takeBottom(64, static_cast<int16_t>(metrics.verticalSpacing)));
  }

  fui::TextStyle title = screen.theme().titleText;
  title.bold = true;
  fui::TextStyle body = screen.theme().bodyText;
  body.maxLines = 3;
  constexpr int16_t detailPaddingY = 18;
  constexpr int16_t detailPaddingX = 20;
  const int16_t titleHeight = screen.target().lineHeight(title.font);
  const int16_t bodyWidth = static_cast<int16_t>(screen.body().width - detailPaddingX * 2);
  const int16_t bodyHeight = fui::measureWrappedText(screen.target(), detailLine, body, bodyWidth).height;
  const int16_t detailHeight =
      static_cast<int16_t>(detailPaddingY * 2 + titleHeight + metrics.verticalSpacing + bodyHeight);
  const fui::Rect detail = screen.takeTop(detailHeight);
  screen.target().stroke(detail, fui::Paint::solid(fui::Color::Black), 2, screen.theme().controlRadius);
  const fui::Rect inset = detail.inset(fui::Insets{detailPaddingY, detailPaddingX, detailPaddingY, detailPaddingX});
  screen.target().text(fui::Rect{inset.x, inset.y, inset.width, static_cast<int16_t>(titleHeight)},
                       destinationLabel(destinations[selected]), title);
  screen.target().text(fui::Rect{inset.x, static_cast<int16_t>(inset.y + titleHeight + metrics.verticalSpacing),
                                 inset.width, bodyHeight},
                       detailLine, body);

  if (!quotePresent) return;
  const fui::Rect band = screen.body();
  fui::TextStyle quoteStyle = screen.theme().bodyText;
  quoteStyle.align = fui::TextAlign::Center;
  quoteStyle.maxLines = 4;
  fui::TextStyle authorStyle = screen.theme().smallText;
  authorStyle.align = fui::TextAlign::Center;
  authorStyle.maxLines = 2;
  const int16_t smallLineHeight = screen.target().lineHeight(authorStyle.font);
  if (band.height < smallLineHeight * 2 + metrics.verticalSpacing) return;

  char displayQuote[GOLF_QUOTE_TEXT_CAPACITY + 7];
  snprintf(displayQuote, sizeof(displayQuote), "\xE2\x80\x9C%s\xE2\x80\x9D", quoteText);
  char displayAuthor[GOLF_QUOTE_AUTHOR_CAPACITY + 5];
  displayAuthor[0] = '\0';
  if (quoteHasAuthor) snprintf(displayAuthor, sizeof(displayAuthor), "\xE2\x80\x94 %s", quoteAuthor);

  const int16_t quoteHeight = fui::measureWrappedText(screen.target(), displayQuote, quoteStyle, band.width).height;
  const int16_t authorHeight =
      quoteHasAuthor ? fui::measureWrappedText(screen.target(), displayAuthor, authorStyle, band.width).height : 0;
  const int16_t interline = quoteHasAuthor ? static_cast<int16_t>(metrics.verticalSpacing) : 0;
  const int16_t contentHeight = static_cast<int16_t>(quoteHeight + interline + authorHeight);
  if (contentHeight > band.height) return;
  const int16_t top = static_cast<int16_t>(band.y + (band.height - contentHeight) / 2);
  screen.target().text(fui::Rect{band.x, top, band.width, quoteHeight}, displayQuote, quoteStyle);
  if (quoteHasAuthor) {
    screen.target().text(
        fui::Rect{band.x, static_cast<int16_t>(top + quoteHeight + interline), band.width, authorHeight}, displayAuthor,
        authorStyle);
  }
}

void GolfHomeActivity::drawFooter() const {
  const char* confirm = resumeFocused                                     ? tr(STR_GOLF_OPEN)
                        : destinations[selected] == Destination::NewRound ? tr(STR_GOLF_START)
                                                                          : tr(STR_GOLF_OPEN);
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), confirm, destinationCount > 1 ? tr(STR_GOLF_BUTTON_PREVIOUS) : "",
                            destinationCount > 1 ? tr(STR_GOLF_BUTTON_NEXT) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfHomeActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle());
  renderUi();
  drawFooter();
  renderer.displayBuffer();
}

#endif
