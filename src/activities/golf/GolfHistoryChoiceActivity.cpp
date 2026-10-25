#include "GolfHistoryChoiceActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <FreeInkUIIcon.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfCareerStatsActivity.h"
#include "GolfHistoryActivity.h"
#include "GolfNavigation.h"
#include "GolfTiles.h"
#include "GolfTrendsActivity.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "components/icons/golfTileIcons.h"
#include "golf/GolfHistory.h"
#include "golf/GolfIndexMigrate.h"
#include "golf/RoundArchive.h"

namespace fui = freeink::ui;

namespace {

constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";

// One checked allocation for the on-entry index pass; freed when onEnter returns.
struct RoundCountScratch {
  GolfPlayerNamesReader reader{};
  GolfIndexMigrator recovery{};
  char chunk[128]{};
};

}  // namespace

GolfHistoryChoiceActivity::GolfHistoryChoiceActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                     const uint8_t playerSlot, const char* playerName)
    : Activity("GolfHistoryChoice", renderer, mappedInput), UiAppHost(renderer), playerSlot(playerSlot) {
  snprintf(this->playerName, sizeof(this->playerName), "%s", playerName != nullptr ? playerName : "");
}

uint32_t GolfHistoryChoiceActivity::readRoundCount() const {
  auto scratch = makeUniqueNoThrow<RoundCountScratch>();
  if (!scratch) {
    LOG_ERR("GOLF", "OOM: history choice round-count scratch (%u bytes)",
            static_cast<unsigned>(sizeof(RoundCountScratch)));
    return 0;
  }
  if (!RoundArchive::recoverIndex(scratch->recovery)) {
    LOG_ERR("GOLF", "History choice refused unrecovered index.csv");
    return 0;
  }
  if (!Storage.exists(INDEX_PATH)) return 0;
  HalFile file;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) return 0;
  while (file.available() > 0) {
    const int bytesRead = file.read(scratch->chunk, sizeof(scratch->chunk));
    if (bytesRead <= 0) return 0;
    scratch->reader.feed(scratch->chunk, static_cast<size_t>(bytesRead));
  }
  scratch->reader.finish();
  return playerSlot < GolfRound::MAX_PLAYERS ? scratch->reader.roundCount(playerSlot) : 0;
}

void GolfHistoryChoiceActivity::onEnter() {
  golfFormatPlayerLabel(playerSlot, playerName, tr(STR_GOLF_PLAYER_LABEL_FORMAT), playerLabel, sizeof(playerLabel));
  snprintf(roundsDetail, sizeof(roundsDetail), tr(STR_GOLF_ROUNDS_RECORDED_FORMAT),
           static_cast<unsigned long>(readRoundCount()));
  selected = 0;

  Activity::onEnter();
  resetUi();
  app.on(ACTION_TILE, &GolfHistoryChoiceActivity::actionTrampoline, this);
  app.setScreen(&GolfHistoryChoiceActivity::screenTrampoline, this);
  requestUpdate();
}

const char* GolfHistoryChoiceActivity::tileLabel(const uint8_t index) const {
  switch (index) {
    case 0:
      return tr(STR_GOLF_TRENDS);
    case 1:
      return tr(STR_GOLF_STATS);
    case 2:
    default:
      return tr(STR_GOLF_ROUNDS);
  }
}

const char* GolfHistoryChoiceActivity::tileDetail(const uint8_t index) const {
  switch (index) {
    case 0:
      return tr(STR_GOLF_CHOOSE_TRENDS_DETAIL);
    case 1:
      return tr(STR_GOLF_CHOOSE_STATS_DETAIL);
    case 2:
    default:
      return roundsDetail;
  }
}

fui::BitmapRef GolfHistoryChoiceActivity::tileIcon(const uint8_t index) const {
  switch (index) {
    case 0:
      return fui::bitmapFromIcon(icon_trending_up_32);
    case 1:
      return fui::bitmapFromIcon(icon_chart_column_32);
    case 2:
    default:
      return fui::bitmapFromIcon(icon_list_32);
  }
}

void GolfHistoryChoiceActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index == 0) {
    auto trends = makeUniqueNoThrow<GolfTrendsActivity>(renderer, mappedInput, playerSlot, playerName);
    if (!trends) {
      LOG_ERR("GOLF", "OOM: trends activity");
      return;
    }
    startActivityForResult(std::move(trends), nullptr);
    return;
  }
  if (index == 1) {
    auto stats = makeUniqueNoThrow<GolfCareerStatsActivity>(renderer, mappedInput, playerSlot, playerName);
    if (!stats) {
      LOG_ERR("GOLF", "OOM: career stats activity");
      return;
    }
    startActivityForResult(std::move(stats), nullptr);
    return;
  }
  if (index == 2) {
    auto history = makeUniqueNoThrow<GolfHistoryActivity>(renderer, mappedInput, playerSlot, playerName);
    if (!history) {
      LOG_ERR("GOLF", "OOM: history activity");
      return;
    }
    startActivityForResult(std::move(history), nullptr);
    return;
  }
}

void GolfHistoryChoiceActivity::moveSelection(const int delta) {
  {
    RenderLock lock(*this);
    selected = static_cast<uint8_t>((selected + TILE_COUNT + delta) % TILE_COUNT);
  }
  requestUpdate();
}

void GolfHistoryChoiceActivity::loop() {
  golfui::FooterInput input(mappedInput,
                            {tr(STR_BACK), tr(STR_GOLF_OPEN), tr(STR_GOLF_BUTTON_PREVIOUS), tr(STR_GOLF_BUTTON_NEXT)});
  if (input.wasReleased(golfui::FooterAction::Back)) {
    finish();
    return;
  }
  if (input.wasReleased(golfui::FooterAction::Confirm)) {
    activateIndex(selected);
    return;
  }
  const int frontDelta = input.releasedFrontNavigationDelta();
  if (frontDelta != 0) {
    moveSelection(frontDelta);
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

void GolfHistoryChoiceActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<GolfHistoryChoiceActivity*>(user)->buildScreen(screen);
}

void GolfHistoryChoiceActivity::actionTrampoline(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<GolfHistoryChoiceActivity*>(user);
  if (event.action != ACTION_TILE || event.value < 0 || event.value >= TILE_COUNT) return;
  {
    RenderLock lock(*self);
    self->selected = static_cast<uint8_t>(event.value);
  }
  self->activateIndex(self->selected);
}

void GolfHistoryChoiceActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, mappedInput, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  const fui::Rect tiles = screen.takeTop(112, static_cast<int16_t>(metrics.verticalSpacing));
  for (uint8_t index = 0; index < TILE_COUNT; ++index) {
    golfui::drawTile(screen, golfui::tileCell(tiles, TILE_COUNT, index), ACTION_TILE, index, index == selected,
                     tileIcon(index), tileLabel(index));
  }

  golfui::drawDetail(screen, tileLabel(selected), tileDetail(selected), metrics.verticalSpacing);
}

void GolfHistoryChoiceActivity::drawFooter() const {
  golfui::drawFooter(renderer, mappedInput,
                     {tr(STR_BACK), tr(STR_GOLF_OPEN), tr(STR_GOLF_BUTTON_PREVIOUS), tr(STR_GOLF_BUTTON_NEXT)});
}

void GolfHistoryChoiceActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, mappedInput, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, playerLabel);
  renderUi();
  drawFooter();
  renderer.displayBuffer();
}

#endif
