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
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateIndex(selected);
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
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  // Tile strip and detail box copy the main menu's constants verbatim
  // (GolfHomeActivity::buildScreen, CONTRACTS-V2 §25.3 / §34.3).
  const fui::Rect tiles = screen.takeTop(112, static_cast<int16_t>(metrics.verticalSpacing));
  constexpr int16_t tileIconSize = 32;
  for (uint8_t index = 0; index < TILE_COUNT; ++index) {
    const int left = tiles.x + static_cast<int32_t>(tiles.width) * index / TILE_COUNT;
    const int right = tiles.x + static_cast<int32_t>(tiles.width) * (index + 1) / TILE_COUNT;
    const fui::Rect tileRect{static_cast<int16_t>(left), tiles.y, static_cast<int16_t>(right - left), tiles.height};
    const bool tileSelected = index == selected;

    fui::ButtonProps tile{};
    tile.action = ACTION_TILE;
    tile.value = index;
    tile.inputMask = fui::InputTouch;
    tile.state = tileSelected ? fui::StateSelected : fui::StateNormal;
    screen.button(tile, tileRect);  // box, selection fill and hit target only

    const fui::Color ink = tileSelected ? fui::Color::White : fui::Color::Black;
    const fui::BitmapRef icon = tileIcon(index);
    const int16_t iconTop = static_cast<int16_t>(tileRect.y + 16);
    if (icon) {
      screen.target().bitmap(fui::Rect{static_cast<int16_t>(tileRect.x + (tileRect.width - tileIconSize) / 2), iconTop,
                                       tileIconSize, tileIconSize},
                             icon, fui::BitmapMode::Center, fui::Paint::solid(ink));
    }
    fui::TextStyle labelStyle = screen.theme().bodyText;
    labelStyle.bold = true;
    labelStyle.align = fui::TextAlign::Center;
    labelStyle.color = ink;
    labelStyle.maxLines = 2;
    const int16_t labelTop = static_cast<int16_t>(iconTop + tileIconSize + 6);
    screen.target().text(
        fui::Rect{tileRect.x, labelTop, tileRect.width, static_cast<int16_t>(tileRect.bottom() - labelTop - 4)},
        tileLabel(index), labelStyle);
  }

  fui::TextStyle title = screen.theme().titleText;
  title.bold = true;
  fui::TextStyle body = screen.theme().bodyText;
  body.maxLines = 3;
  constexpr int16_t detailPaddingY = 18;
  constexpr int16_t detailPaddingX = 20;
  const int16_t titleHeight = screen.target().lineHeight(title.font);
  const int16_t bodyWidth = static_cast<int16_t>(screen.body().width - detailPaddingX * 2);
  const int16_t bodyHeight = fui::measureWrappedText(screen.target(), tileDetail(selected), body, bodyWidth).height;
  const int16_t detailHeight =
      static_cast<int16_t>(detailPaddingY * 2 + titleHeight + metrics.verticalSpacing + bodyHeight);
  const fui::Rect detail = screen.takeTop(detailHeight);
  screen.target().stroke(detail, fui::Paint::solid(fui::Color::Black), 2, screen.theme().controlRadius);
  const fui::Rect inset = detail.inset(fui::Insets{detailPaddingY, detailPaddingX, detailPaddingY, detailPaddingX});
  screen.target().text(fui::Rect{inset.x, inset.y, inset.width, static_cast<int16_t>(titleHeight)}, tileLabel(selected),
                       title);
  screen.target().text(fui::Rect{inset.x, static_cast<int16_t>(inset.y + titleHeight + metrics.verticalSpacing),
                                 inset.width, bodyHeight},
                       tileDetail(selected), body);
}

void GolfHistoryChoiceActivity::drawFooter() const {
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), tr(STR_GOLF_OPEN), tr(STR_GOLF_BUTTON_PREVIOUS), tr(STR_GOLF_BUTTON_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfHistoryChoiceActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, playerLabel);
  renderUi();
  drawFooter();
  renderer.displayBuffer();
}

#endif
