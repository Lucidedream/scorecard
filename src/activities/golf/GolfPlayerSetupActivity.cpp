#include "GolfPlayerSetupActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>
#include <variant>

#include "GolfNavigation.h"
#include "GolfUiLayout.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "golf/GolfRoundStore.h"
#include "golf/GolfRules.h"

namespace fui = freeink::ui;

static_assert(sizeof(GolfCourse) <= 256, "Tee selection must stay within the embedded stack budget");

GolfPlayerSetupActivity::GolfPlayerSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 const GolfCourseFile& selectedCourseFile,
                                                 const GolfCourse& selectedCourse)
    : UiListActivity("GolfPlayerSetup", renderer, mappedInput),
      courseFile(selectedCourseFile),
      course(selectedCourse) {
  CourseStore::applyGolfCourse(course, draft, 0);
  CourseStore::initializeGolfPlayerSelection(courseFile, course, draft);
}

void GolfPlayerSetupActivity::onEnter() {
  phase = Phase::Players;
  editingPlayer = 0;
  saveFailed = false;
  teeResolutionFailed = false;
  refreshPlayerRows();
  initializeTeeRows();
  UiListActivity::onEnter();
  nav.reset(golfPlayerIsEnabled(draft.players[0]) ? 0 : 1);
}

int GolfPlayerSetupActivity::listCount() const {
  return phase == Phase::Players ? PLAYER_ROW_COUNT : TEE_OPTION_COUNT;
}

const char* GolfPlayerSetupActivity::headerTitle() const {
  if (saveFailed) return tr(STR_GOLF_SAVE_ERROR);
  if (teeResolutionFailed) return tr(STR_GOLF_TEE_UNAVAILABLE_ERROR);
  return phase == Phase::Players ? tr(STR_GOLF_PLAYER_SETUP) : teeChoicePlayerLabel;
}

const char* GolfPlayerSetupActivity::teeLabel(const TeeSelection tee) {
  switch (tee) {
    case TeeSelection::Blue:
      return tr(STR_GOLF_BLUE);
    case TeeSelection::White:
      return tr(STR_GOLF_WHITE);
    case TeeSelection::NotPlay:
    default:
      return tr(STR_GOLF_NOT_PLAY);
  }
}

void GolfPlayerSetupActivity::refreshPlayerRows() {
  for (uint8_t player = 0; player < GolfRound::MAX_PLAYERS; ++player) {
    const uint8_t nameRow = player * 2;
    const uint8_t teeRow = nameRow + 1;

    playerRows[nameRow] = {};
    playerRows[nameRow].label = tr(STR_GOLF_PLAYER);
    playerRows[nameRow].value = draft.players[player].name;
    playerRows[nameRow].actionValue = nameRow;
    playerRows[nameRow].enabled = golfPlayerIsEnabled(draft.players[player]);

    playerRows[teeRow] = {};
    playerRows[teeRow].label = tr(STR_GOLF_TEE);
    playerRows[teeRow].value = teeLabel(draft.players[player].tee);
    playerRows[teeRow].actionValue = teeRow;
    playerRows[teeRow].enabled = true;
  }

  playerRows[COMPLETE_ROW] = {};
  playerRows[COMPLETE_ROW].label = tr(STR_GOLF_COMPLETE);
  playerRows[COMPLETE_ROW].actionValue = COMPLETE_ROW;
  playerRows[COMPLETE_ROW].enabled = hasEnabledPlayer();
}

void GolfPlayerSetupActivity::initializeTeeRows() {
  teeRows[0] = {};
  teeRows[0].label = tr(STR_GOLF_NOT_PLAY);
  teeRows[0].actionValue = 0;
  teeRows[0].enabled = true;

  GolfTeeResolution resolved{};
  teeRows[1] = {};
  teeRows[1].label = tr(STR_GOLF_BLUE);
  teeRows[1].actionValue = 1;
  teeRows[1].enabled = CourseStore::resolveTee(courseFile, course, TeeSelection::Blue, resolved);

  teeRows[2] = {};
  teeRows[2].label = tr(STR_GOLF_WHITE);
  teeRows[2].actionValue = 2;
  teeRows[2].enabled = CourseStore::resolveTee(courseFile, course, TeeSelection::White, resolved);
}

bool GolfPlayerSetupActivity::hasEnabledPlayer() const {
  for (const GolfPlayer& player : draft.players) {
    if (golfPlayerIsEnabled(player)) return true;
  }
  return false;
}

bool GolfPlayerSetupActivity::rowIsEnabled(const int index) const {
  if (index < 0 || index >= listCount()) return false;
  return phase == Phase::Players ? playerRows[index].enabled : teeRows[index].enabled;
}

int GolfPlayerSetupActivity::nextFocusableIndex(const int current, const int direction) const {
  const int count = listCount();
  if (count <= 0) return 0;
  int candidate = current;
  for (int checked = 0; checked < count; ++checked) {
    candidate = (candidate + direction + count) % count;
    if (rowIsEnabled(candidate)) return candidate;
  }
  return current;
}

void GolfPlayerSetupActivity::navigateButtons() {
  buttonNavigator.onNextRelease([this] { moveSelectionTo(nextFocusableIndex(nav.selected, 1)); });
  buttonNavigator.onPreviousRelease([this] { moveSelectionTo(nextFocusableIndex(nav.selected, -1)); });
  buttonNavigator.onNextContinuous([this] { moveSelectionTo(nextFocusableIndex(nav.selected, 1)); });
  buttonNavigator.onPreviousContinuous([this] { moveSelectionTo(nextFocusableIndex(nav.selected, -1)); });
}

void GolfPlayerSetupActivity::onRowAction(const fui::ActionEvent& event) {
  if (!rowIsEnabled(event.value)) return;
  moveSelectionTo(event.value);
  activateIndex(event.value);
}

void GolfPlayerSetupActivity::activateIndex(const int index) {
  app.clearTapFlash();
  // UiListActivity's Confirm path is index-based, so enforce the same enabled
  // gate that FreeInkUI applies to touch hit registration.
  if (!rowIsEnabled(index)) return;

  if (phase == Phase::TeeChoice) {
    switch (index) {
      case 0:
        selectTee(TeeSelection::NotPlay);
        break;
      case 1:
        selectTee(TeeSelection::Blue);
        break;
      case 2:
        selectTee(TeeSelection::White);
        break;
      default:
        break;
    }
    return;
  }

  if (index == COMPLETE_ROW) {
    completeRound();
    return;
  }

  const uint8_t player = static_cast<uint8_t>(index / 2);
  if ((index % 2) == 0) {
    editPlayerName(player);
  } else {
    openTeeChoice(player);
  }
}

void GolfPlayerSetupActivity::openTeeChoice(const uint8_t player) {
  if (player >= GolfRound::MAX_PLAYERS) return;
  {
    RenderLock lock(*this);
    editingPlayer = player;
    phase = Phase::TeeChoice;
    saveFailed = false;
    teeResolutionFailed = false;
    golfFormatPlayerLabel(player, draft.players[player].name, tr(STR_GOLF_PLAYER_LABEL_FORMAT),
                          teeChoicePlayerLabel, sizeof(teeChoicePlayerLabel));
    closeRouting();
    const int selected = static_cast<int>(draft.players[player].tee);
    nav.reset(rowIsEnabled(selected) ? selected : nextFocusableIndex(selected, 1));
  }
  requestUpdate();
}

void GolfPlayerSetupActivity::selectTee(const TeeSelection tee) {
  {
    RenderLock lock(*this);
    draft.players[editingPlayer].tee = tee;
    phase = Phase::Players;
    saveFailed = false;
    teeResolutionFailed = false;
    refreshPlayerRows();
    closeRouting();
    nav.reset(static_cast<int>(editingPlayer * 2 + 1));
  }
  requestUpdate();
}

void GolfPlayerSetupActivity::returnToPlayers() {
  {
    RenderLock lock(*this);
    phase = Phase::Players;
    saveFailed = false;
    teeResolutionFailed = false;
    closeRouting();
    nav.reset(static_cast<int>(editingPlayer * 2 + 1));
  }
  requestUpdate();
}

void GolfPlayerSetupActivity::onBackButton() {
  if (phase == Phase::TeeChoice) {
    returnToPlayers();
    return;
  }
  openGolfSetup(activityManager, renderer, mappedInput);
}

bool GolfPlayerSetupActivity::applyPlayerName(const uint8_t player, const std::string_view name) {
  if (player >= GolfRound::MAX_PLAYERS || !golfPlayerNameHasVisibleText(name)) return false;
  GolfPlayer& target = draft.players[player];
  const size_t length = golfUtf8PrefixLength(name, GolfPlayer::NAME_CAPACITY - 1);
  if (length == 0) return false;
  memset(target.name, 0, sizeof(target.name));
  memcpy(target.name, name.data(), length);
  return true;
}

void GolfPlayerSetupActivity::editPlayerName(const uint8_t player) {
  if (player >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(draft.players[player])) return;

  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(
      renderer, mappedInput, tr(STR_GOLF_PLAYER_NAME), draft.players[player].name,
      static_cast<size_t>(GolfPlayer::NAME_CAPACITY - 1), InputType::Text);
  if (!keyboard) {
    LOG_ERR("GOLF", "OOM: player name keyboard");
    return;
  }

  startActivityForResult(std::move(keyboard), [this, player](const ActivityResult& result) {
    if (result.isCancelled || !std::holds_alternative<KeyboardResult>(result.data)) return;
    const std::string& entered = std::get<KeyboardResult>(result.data).text;
    if (!golfPlayerNameHasVisibleText(entered)) return;
    RenderLock lock(*this);
    if (!applyPlayerName(player, entered)) return;
    saveFailed = false;
    teeResolutionFailed = false;
    refreshPlayerRows();
  });
}

void GolfPlayerSetupActivity::completeRound() {
  bool teesValid = true;
  {
    RenderLock lock(*this);
    const uint8_t firstPlayer = golfFirstEnabledPlayer(draft);
    if (firstPlayer == GolfRound::NO_PLAYER) return;

    // Resolve the whole group before changing any yardages. Four lightweight
    // pointer/flag records stay well below the embedded stack budget and avoid
    // a second resolver pass or a large per-player GolfCourse copy.
    GolfTeeResolution resolved[GolfRound::MAX_PLAYERS]{};
    for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
      const GolfPlayer& player = draft.players[slot];
      if (!golfPlayerIsEnabled(player)) continue;
      if (!CourseStore::resolveTee(courseFile, course, player.tee, resolved[slot])) {
        LOG_ERR("GOLF", "Unavailable tee %u for player slot %u", static_cast<unsigned>(player.tee),
                static_cast<unsigned>(slot));
        teeResolutionFailed = true;
        saveFailed = false;
        teesValid = false;
        break;
      }
    }

    if (teesValid) {
      draft.currentHole = 0;
      draft.currentPlayer = firstPlayer;
      for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
        GolfPlayer& player = draft.players[slot];
        memset(player.yards, 0, sizeof(player.yards));
        if (golfPlayerIsEnabled(player) && resolved[slot].hasYards) {
          memcpy(player.yards, resolved[slot].yards, sizeof(player.yards));
        }
      }
      saveFailed = false;
      teeResolutionFailed = false;
    }
  }

  if (!teesValid) {
    requestUpdate();
    return;
  }

  GOLF_ROUND_STORE.setRound(draft);
  if (!GOLF_ROUND_STORE.saveToFile()) {
    {
      RenderLock lock(*this);
      saveFailed = true;
    }
    requestUpdate();
    return;
  }
  openGolfScoring(activityManager, renderer, mappedInput);
}

void GolfPlayerSetupActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding, metrics.headerHeight);
  GUI.drawHeader(renderer, Rect{layout.header.x, layout.header.y, layout.header.width, layout.header.height},
                 headerTitle(), phase == Phase::TeeChoice ? tr(STR_GOLF_CHOOSE_TEE) : nullptr);
}

void GolfPlayerSetupActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding,
                                            metrics.headerHeight);
  screen.setContentMargin(layout.contentMargins);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  listProps = {};
  listProps.items = phase == Phase::Players ? playerRows : teeRows;
  listProps.count = static_cast<uint16_t>(listCount());
  listProps.action = ACTION_ROW;
  listProps.inputMask = fui::InputTouch;
  listProps.valueInset = 8;
  syncListViewport(screen, listProps);
  screen.list(listProps);
}

#endif
