#include "GolfHomeActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <cstdio>

#include "GolfNavigation.h"
#include "GolfPlayerSelectActivity.h"
#include "GolfSetupActivity.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "golf/GolfRoundStore.h"
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
  destinations[destinationCount++] = Destination::Trends;
  selected = 0;
  resumeFocused = hasOpenRound;
  scanIndexSummary();
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
    case Destination::Trends:
    default:
      return tr(STR_GOLF_TRENDS);
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
    case Destination::Trends:
      snprintf(detailLine, sizeof(detailLine), tr(STR_GOLF_TRENDS_SUMMARY_FORMAT),
               static_cast<unsigned long>(indexSummary.totalRounds()), indexSummary.playerCount());
      return;
  }
}

void GolfHomeActivity::activateDestination(const Destination destination) {
  app.clearTapFlash();
  switch (destination) {
    case Destination::NewRound:
      openGolfSetup(activityManager, renderer, mappedInput);
      return;
    case Destination::History:
    case Destination::Trends:
      break;
  }
  const auto mode = destination == Destination::History ? GolfPlayerSelectActivity::Mode::History
                                                        : GolfPlayerSelectActivity::Mode::Trends;
  auto selector = makeUniqueNoThrow<GolfPlayerSelectActivity>(renderer, mappedInput, mode);
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
  const fui::Rect tiles = screen.takeTop(120, static_cast<int16_t>(metrics.verticalSpacing));
  for (uint8_t index = 0; index < destinationCount; ++index) {
    const int left = tiles.x + static_cast<int32_t>(tiles.width) * index / destinationCount;
    const int right = tiles.x + static_cast<int32_t>(tiles.width) * (index + 1) / destinationCount;
    fui::ButtonProps tile{};
    tile.label = destinationLabel(destinations[index]);
    tile.action = ACTION_TILE;
    tile.value = index;
    tile.inputMask = fui::InputTouch;
    tile.state = !resumeFocused && index == selected ? fui::StateSelected : fui::StateNormal;
    tile.text = screen.theme().bodyText;
    tile.text.bold = true;
    screen.button(tile,
                  fui::Rect{static_cast<int16_t>(left), tiles.y, static_cast<int16_t>(right - left), tiles.height});
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

  const fui::Rect detail = screen.body();
  screen.target().stroke(detail, fui::Paint::solid(fui::Color::Black), 2, screen.theme().controlRadius);
  const fui::Rect inset = detail.inset(fui::Insets{18, 20, 18, 20});
  fui::TextStyle title = screen.theme().titleText;
  title.bold = true;
  const int titleHeight = screen.target().lineHeight(title.font);
  screen.target().text(fui::Rect{inset.x, inset.y, inset.width, static_cast<int16_t>(titleHeight)},
                       destinationLabel(destinations[selected]), title);
  fui::TextStyle body = screen.theme().bodyText;
  body.maxLines = 3;
  screen.target().text(
      fui::Rect{inset.x, static_cast<int16_t>(inset.y + titleHeight + metrics.verticalSpacing), inset.width,
                static_cast<int16_t>(inset.height - titleHeight - metrics.verticalSpacing)},
      detailLine, body);
}

void GolfHomeActivity::drawFooter() const {
  const char* confirm = resumeFocused                                     ? tr(STR_GOLF_OPEN)
                        : destinations[selected] == Destination::NewRound ? tr(STR_GOLF_START)
                                                                          : tr(STR_GOLF_OPEN);
  const auto labels =
      mappedInput.mapLabels(tr(STR_GOLF_BUTTON_BACK), confirm, destinationCount > 1 ? tr(STR_GOLF_BUTTON_PREVIOUS) : "",
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
