#include "GolfPlayerSetupActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>
#include <variant>

#include "GolfNavigation.h"
#include "GolfPlayerSetupPolicy.h"
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
    : UiListActivity("GolfPlayerSetup", renderer, mappedInput), courseFile(selectedCourseFile), course(selectedCourse) {
  CourseStore::applyGolfCourse(course, draft, 0);
  CourseStore::initializeGolfPlayerSelection(courseFile, course, draft);
  defaultTee = draft.players[0].tee;
}

void GolfPlayerSetupActivity::onEnter() {
  phase = Phase::Count;
  golfSetPlayerCount(draft, playerCount, 1, defaultTee);
  editingPlayer = 0;
  saveFailed = false;
  teeResolutionFailed = false;
  refreshPlayerRows();
  initializeTeeRows();
  UiListActivity::onEnter();
  nav.reset();
}

int GolfPlayerSetupActivity::listCount() const {
  if (phase == Phase::Count) return 0;
  return phase == Phase::Players ? playerCount : TEE_OPTION_COUNT;
}

const char* GolfPlayerSetupActivity::headerTitle() const {
  if (saveFailed) return tr(STR_GOLF_SAVE_ERROR);
  if (teeResolutionFailed) return tr(STR_GOLF_TEE_UNAVAILABLE_ERROR);
  if (phase == Phase::Count) return course.courseName;
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
    playerRows[player] = {};
    playerRows[player].label = draft.players[player].name;
    playerRows[player].value = teeLabel(draft.players[player].tee);
    playerRows[player].actionValue = player;
    playerRows[player].enabled = player < playerCount;
  }
}

void GolfPlayerSetupActivity::initializeTeeRows() {
  teeRows[0] = {};
  teeRows[0].label = tr(STR_GOLF_PLAYER_NAME);
  teeRows[0].value = draft.players[editingPlayer].name;
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
  if (phase == Phase::Count) {
    buttonNavigator.onNextRelease([this] { stepPlayerCount(1); });
    buttonNavigator.onPreviousRelease([this] { stepPlayerCount(-1); });
    buttonNavigator.onNextContinuous([this] { stepPlayerCount(1); });
    buttonNavigator.onPreviousContinuous([this] { stepPlayerCount(-1); });
    return;
  }
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
        editPlayerName(editingPlayer);
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

  openTeeChoice(static_cast<uint8_t>(index));
}

void GolfPlayerSetupActivity::stepPlayerCount(const int direction) {
  const uint8_t next = golfStepPlayerCount(playerCount, direction);
  if (next == playerCount) return;
  {
    RenderLock lock(*this);
    golfSetPlayerCount(draft, playerCount, next, defaultTee);
    saveFailed = false;
    teeResolutionFailed = false;
  }
  requestUpdate();
}

void GolfPlayerSetupActivity::showPlayers() {
  {
    RenderLock lock(*this);
    phase = Phase::Players;
    saveFailed = false;
    teeResolutionFailed = false;
    refreshPlayerRows();
    closeRouting();
    nav.reset();
  }
  requestUpdate();
}

void GolfPlayerSetupActivity::openTeeChoice(const uint8_t player) {
  if (player >= GolfRound::MAX_PLAYERS) return;
  {
    RenderLock lock(*this);
    editingPlayer = player;
    phase = Phase::TeeChoice;
    saveFailed = false;
    teeResolutionFailed = false;
    initializeTeeRows();
    golfFormatPlayerLabel(player, draft.players[player].name, tr(STR_GOLF_PLAYER_LABEL_FORMAT), teeChoicePlayerLabel,
                          sizeof(teeChoicePlayerLabel));
    closeRouting();
    const int selected = static_cast<int>(draft.players[player].tee);
    nav.reset(rowIsEnabled(selected) ? selected : nextFocusableIndex(selected, 1));
  }
  requestUpdate();
}

void GolfPlayerSetupActivity::selectTee(const TeeSelection tee) {
  if (tee == TeeSelection::NotPlay) return;
  {
    RenderLock lock(*this);
    draft.players[editingPlayer].tee = tee;
    phase = Phase::Players;
    saveFailed = false;
    teeResolutionFailed = false;
    refreshPlayerRows();
    closeRouting();
    nav.reset(editingPlayer);
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
    nav.reset(editingPlayer);
  }
  requestUpdate();
}

void GolfPlayerSetupActivity::onBackButton() {
  if (phase == Phase::TeeChoice) {
    returnToPlayers();
    return;
  }
  if (phase == Phase::Players) {
    {
      RenderLock lock(*this);
      phase = Phase::Count;
      saveFailed = false;
      teeResolutionFailed = false;
      closeRouting();
      nav.reset();
    }
    requestUpdate();
    return;
  }
  openGolfSetup(activityManager, renderer, mappedInput);
}

bool GolfPlayerSetupActivity::handleButtons() {
  if (phase == Phase::Players && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    completeRound();
    return true;
  }
  if (phase != Phase::Count) return UiListActivity::handleButtons();
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBackButton();
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (golfPlayerSetupNext(playerCount)) {
      case GolfPlayerSetupNext::ReviewRoster:
        showPlayers();
        break;
    }
    return true;
  }
  return false;
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
    initializeTeeRows();
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
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle(),
                     phase == Phase::TeeChoice ? tr(STR_GOLF_CHOOSE_TEE) : nullptr);
}

void GolfPlayerSetupActivity::drawFooter() {
  const char* confirm = tr(STR_SELECT);
  const char* previous = tr(STR_DIR_UP);
  const char* next = tr(STR_DIR_DOWN);
  if (phase == Phase::Count) {
    confirm = tr(STR_GOLF_NEXT);
    previous = tr(STR_GOLF_BUTTON_PREVIOUS);
    next = tr(STR_GOLF_BUTTON_NEXT);
  } else if (phase == Phase::Players) {
    confirm = tr(STR_GOLF_START);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirm, previous, next);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfPlayerSetupActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  if (phase == Phase::Count) {
    const fui::Rect body = screen.body();
    fui::TextStyle question = screen.theme().titleText;
    question.align = fui::TextAlign::Center;
    question.bold = true;
    fui::TextStyle value = question;
    fui::TextStyle unit = screen.theme().bodyText;
    unit.align = fui::TextAlign::Center;
    unit.bold = true;

    const int questionHeight = screen.target().lineHeight(question.font);
    const int valueHeight = screen.target().lineHeight(value.font);
    const int unitHeight = screen.target().lineHeight(unit.font);
    constexpr int PIP_SIZE = 15;
    constexpr int PIP_GAP = 10;
    constexpr int BLOCK_GAP = 18;
    const int pipsWidth = GOLF_MAX_PLAYERS * PIP_SIZE + (GOLF_MAX_PLAYERS - 1) * PIP_GAP;
    const int blockHeight = questionHeight + valueHeight + unitHeight + PIP_SIZE + BLOCK_GAP * 3;
    int y = body.y + (body.height - blockHeight) / 2;
    screen.target().text(fui::Rect{body.x, static_cast<int16_t>(y), body.width, static_cast<int16_t>(questionHeight)},
                         tr(STR_GOLF_HOW_MANY_PLAYERS), question);
    y += questionHeight + BLOCK_GAP;
    char count[2] = {static_cast<char>('0' + playerCount), '\0'};
    screen.target().text(fui::Rect{body.x, static_cast<int16_t>(y), body.width, static_cast<int16_t>(valueHeight)},
                         count, value);
    y += valueHeight + BLOCK_GAP;
    screen.target().text(fui::Rect{body.x, static_cast<int16_t>(y), body.width, static_cast<int16_t>(unitHeight)},
                         playerCount == 1 ? tr(STR_GOLF_PLAYER) : tr(STR_GOLF_PLAYERS), unit);
    y += unitHeight + BLOCK_GAP;
    const int pipsX = body.x + (body.width - pipsWidth) / 2;
    for (uint8_t pip = 0; pip < GOLF_MAX_PLAYERS; ++pip) {
      const fui::Rect pipRect{static_cast<int16_t>(pipsX + pip * (PIP_SIZE + PIP_GAP)), static_cast<int16_t>(y),
                              PIP_SIZE, PIP_SIZE};
      if (pip < playerCount) {
        screen.target().fill(pipRect, fui::Paint::solid(fui::Color::Black), PIP_SIZE / 2);
      } else {
        screen.target().stroke(pipRect, fui::Paint::solid(fui::Color::Black), 2, PIP_SIZE / 2);
      }
    }
    return;
  }
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
