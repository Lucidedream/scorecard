#include "GolfPlayerSetupActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <FreeInkUIIcon.h>
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
#include "components/icons/golfTileIcons.h"
#include "golf/CourseOrder.h"
#include "golf/GolfRoundStore.h"
#include "golf/GolfRules.h"

namespace fui = freeink::ui;

GolfPlayerSetupActivity::GolfPlayerSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 const GolfCourseFile& selectedCourseFile,
                                                 GolfNineSelection selectedNines)
    : GolfUiListActivity("GolfPlayerSetup", renderer, mappedInput),
      courseFile(selectedCourseFile),
      selection(selectedNines) {}

void GolfPlayerSetupActivity::onEnter() {
  phase = Phase::Count;
  loadFailed = !CourseStore::load(courseFile, course) || !golfComposeRound(course, selection, 0, draft);
  if (loadFailed) {
    LOG_ERR("GOLF", "Player setup: course load failed");
    UiListActivity::onEnter();
    return;
  }
  golfSetTeeString(defaultTee, golfDefaultTee(course));
  golfSetPlayerCount(draft, playerCount, 1, defaultTee);
  editingPlayer = 0;
  saveFailed = false;
  teeResolutionFailed = false;
  refreshPlayerRows();
  initializeTeeRows();
  UiListActivity::onEnter();
  app.on(
      ACTION_USER,
      [](const fui::ActionEvent& event, void* user) {
        static_cast<GolfPlayerSetupActivity*>(user)->onCountTouch(event);
      },
      this);
  nav.reset();
}

uint8_t GolfPlayerSetupActivity::teeRowCount() const {
  const uint8_t tees = course.teeCount < GOLF_MAX_TEES ? course.teeCount : GOLF_MAX_TEES;
  return static_cast<uint8_t>(1 + tees);
}

const GolfCourseTee* GolfPlayerSetupActivity::findTee(const char* name) const {
  if (name == nullptr) return nullptr;
  for (uint8_t i = 0; i < course.teeCount; ++i) {
    if (strcmp(course.tees[i].name, name) == 0) return &course.tees[i];
  }
  return nullptr;
}

int GolfPlayerSetupActivity::listCount() const {
  if (loadFailed || phase == Phase::Count) return 0;
  return phase == Phase::Players ? playerCount + 1 : teeRowCount();
}

const char* GolfPlayerSetupActivity::headerTitle() const {
  if (loadFailed) return tr(STR_GOLF_COURSE_LOAD_ERROR);
  if (saveFailed) return tr(STR_GOLF_SAVE_ERROR);
  if (teeResolutionFailed) return tr(STR_GOLF_TEE_UNAVAILABLE_ERROR);
  if (phase == Phase::Count) return course.courseName;
  return phase == Phase::Players ? tr(STR_GOLF_PLAYER_SETUP) : teeChoicePlayerLabel;
}

void GolfPlayerSetupActivity::refreshPlayerRows() {
  for (uint8_t player = 0; player < GolfRound::MAX_PLAYERS; ++player) {
    playerRows[player] = {};
    playerRows[player].label = draft.players[player].name;
    playerRows[player].value = golfTeeDisplayLabel(draft.players[player].tee);
    playerRows[player].actionValue = player;
    playerRows[player].enabled = player < playerCount;
  }
  playerRows[playerCount] = {};
  playerRows[playerCount].label = mappedInput.hasTouch() ? tr(STR_GOLF_START_ROUND) : tr(STR_GOLF_START);
  playerRows[playerCount].actionValue = playerCount;
  playerRows[playerCount].enabled = true;
}

void GolfPlayerSetupActivity::initializeTeeRows() {
  teeRows[0] = {};
  teeRows[0].label = tr(STR_GOLF_PLAYER_NAME);
  teeRows[0].value = draft.players[editingPlayer].name;
  teeRows[0].actionValue = 0;
  teeRows[0].enabled = true;

  // One row per resolved course tee (CONTRACTS-V2 §32.3), bounded by GOLF_MAX_TEES.
  for (uint8_t i = 0; i < GOLF_MAX_TEES; ++i) {
    freeink::ui::ListItem& row = teeRows[i + 1];
    row = {};
    row.actionValue = i + 1;
    if (i < course.teeCount) {
      row.label = golfTeeDisplayLabel(course.tees[i].name);
      row.enabled = true;
    } else {
      row.enabled = false;
    }
  }
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
  if (loadFailed) return;
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
    if (index == 0) {
      editPlayerName(editingPlayer);
      return;
    }
    const uint8_t teeIndex = static_cast<uint8_t>(index - 1);
    if (teeIndex < course.teeCount) selectTee(course.tees[teeIndex].name);
    return;
  }

  if (phase == Phase::Players && index == playerCount) {
    completeRound();
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

void GolfPlayerSetupActivity::onCountTouch(const fui::ActionEvent& event) {
  if (loadFailed || phase != Phase::Count || !mappedInput.hasTouch()) return;
  if (event.value == 0) {
    app.clearTapFlash();
    onFooterConfirm();
    return;
  }
  if (event.value < 1 || event.value > GOLF_MAX_PLAYERS || event.value == playerCount) return;
  {
    RenderLock lock(*this);
    golfSetPlayerCount(draft, playerCount, static_cast<uint8_t>(event.value), defaultTee);
    saveFailed = false;
    teeResolutionFailed = false;
  }
  requestUpdate();
}

void GolfPlayerSetupActivity::drawTouchCount(UiScreen& screen) {
  auto question = screen.theme().titleText;
  question.align = fui::TextAlign::Center;
  question.bold = true;
  const auto value = question;
  auto unit = screen.theme().bodyText;
  unit.align = fui::TextAlign::Center;
  unit.bold = true;
  const auto layout =
      golfui::makePlayerCountTouchLayout(screen.body(), screen.target().lineHeight(question.font),
                                         screen.target().lineHeight(value.font), screen.target().lineHeight(unit.font));
  screen.target().text(layout.question, tr(STR_GOLF_HOW_MANY_PLAYERS), question);
  const char count[2] = {static_cast<char>('0' + playerCount), '\0'};
  screen.target().text(layout.value, count, value);
  screen.target().text(layout.unit, playerCount == 1 ? tr(STR_GOLF_PLAYER) : tr(STR_GOLF_PLAYERS), unit);
  for (uint8_t index = 0; index < GOLF_MAX_PLAYERS; ++index) {
    const auto dot = golfui::playerCountDotRect(layout, index);
    screen.frame().hit(golfui::playerCountDotHitRect(layout, index), ACTION_USER, index + 1, fui::InputTouch);
    if (index < playerCount)
      screen.target().fill(dot, fui::Paint::solid(fui::Color::Black), dot.width / 2);
    else
      screen.target().stroke(dot, fui::Paint::solid(fui::Color::Black), 2, dot.width / 2);
  }
  screen.frame().hit(layout.next, ACTION_USER, 0, fui::InputTouch);
  screen.target().fill(layout.next, fui::Paint::solid(fui::Color::Black));
  auto nextText = screen.theme().bodyText;
  nextText.bold = true;
  nextText.color = fui::Color::White;
  const char* label = tr(STR_GOLF_NEXT);
  const int labelWidth = screen.target().measureText(nextText.font, label, nextText).width;
  constexpr int ICON_SIZE = 24;
  constexpr int ICON_GAP = 8;
  const int x = layout.next.x + (layout.next.width - labelWidth - ICON_GAP - ICON_SIZE) / 2;
  const int lineHeight = screen.target().lineHeight(nextText.font);
  nextText.align = fui::TextAlign::Left;
  screen.target().text(
      {static_cast<int16_t>(x), static_cast<int16_t>(layout.next.y + (layout.next.height - lineHeight) / 2),
       static_cast<int16_t>(labelWidth), static_cast<int16_t>(lineHeight)},
      label, nextText);
  screen.target().bitmap(
      {static_cast<int16_t>(x + labelWidth + ICON_GAP),
       static_cast<int16_t>(layout.next.y + (layout.next.height - ICON_SIZE) / 2), ICON_SIZE, ICON_SIZE},
      fui::bitmapFromIcon(icon_chevron_right_24), fui::BitmapMode::Center, fui::Paint::solid(fui::Color::White));
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
    // Pre-select the row whose tee name matches this player's current tee.
    int selected = 0;
    for (uint8_t i = 0; i < course.teeCount; ++i) {
      if (strcmp(draft.players[player].tee, course.tees[i].name) == 0) {
        selected = i + 1;
        break;
      }
    }
    nav.reset(rowIsEnabled(selected) ? selected : nextFocusableIndex(selected, 1));
  }
  requestUpdate();
}

void GolfPlayerSetupActivity::selectTee(const char* tee) {
  if (tee == nullptr || tee[0] == '\0') return;
  {
    RenderLock lock(*this);
    golfSetTee(draft.players[editingPlayer], tee);
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
  if (!loadFailed && course.nineCount >= 2) {
    openGolfNineSelect(activityManager, renderer, mappedInput, courseFile, selection);
  } else {
    openGolfSetup(activityManager, renderer, mappedInput);
  }
}

void GolfPlayerSetupActivity::onFooterConfirm() {
  if (loadFailed) return;
  if (phase == Phase::Count) {
    switch (golfPlayerSetupNext(playerCount)) {
      case GolfPlayerSetupNext::ReviewRoster:
        showPlayers();
        break;
    }
    return;
  }
  GolfUiListActivity::onFooterConfirm();
}

void GolfPlayerSetupActivity::onFooterPrevious() {
  if (loadFailed) return;
  if (phase == Phase::Count) {
    stepPlayerCount(-1);
    return;
  }
  GolfUiListActivity::onFooterPrevious();
}

void GolfPlayerSetupActivity::onFooterNext() {
  if (loadFailed) return;
  if (phase == Phase::Count) {
    stepPlayerCount(1);
    return;
  }
  GolfUiListActivity::onFooterNext();
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

    // Validate every tee before changing player yardages.
    const GolfCourseTee* playerTee[GolfRound::MAX_PLAYERS]{};
    for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
      const GolfPlayer& player = draft.players[slot];
      if (!golfPlayerIsEnabled(player)) continue;
      playerTee[slot] = findTee(player.tee);
      if (playerTee[slot] == nullptr) {
        LOG_ERR("GOLF", "Unavailable tee %s for player slot %u", player.tee, static_cast<unsigned>(slot));
        teeResolutionFailed = true;
        saveFailed = false;
        teesValid = false;
        break;
      }
    }

    if (teesValid) {
      struct PlayerSelection {
        char name[GolfPlayer::NAME_CAPACITY];
        char tee[GOLF_TEE_CAPACITY];
      };
      PlayerSelection players[GOLF_MAX_PLAYERS]{};
      for (uint8_t slot = 0; slot < GOLF_MAX_PLAYERS; ++slot) {
        memcpy(players[slot].name, draft.players[slot].name, sizeof(players[slot].name));
        memcpy(players[slot].tee, draft.players[slot].tee, sizeof(players[slot].tee));
      }
      if (!golfComposeRound(course, selection, draft.dateYmd, draft)) {
        LOG_ERR("GOLF", "Invalid nine selection at round creation");
        return;
      }
      for (uint8_t slot = 0; slot < GOLF_MAX_PLAYERS; ++slot) {
        memcpy(draft.players[slot].name, players[slot].name, sizeof(players[slot].name));
        memcpy(draft.players[slot].tee, players[slot].tee, sizeof(players[slot].tee));
      }
      draft.currentHole = 0;
      draft.currentPlayer = firstPlayer;
      for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
        GolfPlayer& player = draft.players[slot];
        memset(player.yards, 0, sizeof(player.yards));
        if (golfPlayerIsEnabled(player) && playerTee[slot] != nullptr && playerTee[slot]->hasYards) {
          golfComposeTeeYards(course, static_cast<uint8_t>(playerTee[slot] - course.tees), selection, player.yards);
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
  const auto layout = golfui::chromeLayout(renderer, mappedInput, metrics.topPadding, {}, keepTouchFooter());
  golfui::drawHeader(renderer, layout.header, headerTitle(),
                     phase == Phase::TeeChoice ? tr(STR_GOLF_CHOOSE_TEE) : nullptr);
}

golfui::FooterLabels GolfPlayerSetupActivity::golfFooterLabels() const {
  if (loadFailed) return {tr(STR_BACK), "", "", ""};
  const char* confirm = tr(STR_SELECT);
  const char* previous = tr(STR_DIR_UP);
  const char* next = tr(STR_DIR_DOWN);
  if (phase == Phase::Count) {
    confirm = tr(STR_GOLF_NEXT);
    previous = tr(STR_GOLF_BUTTON_PREVIOUS);
    next = tr(STR_GOLF_BUTTON_NEXT);
  } else if (phase == Phase::Players && nav.selected == playerCount) {
    confirm = tr(STR_GOLF_START);
  }
  return {tr(STR_BACK), confirm, previous, next};
}

void GolfPlayerSetupActivity::buildScreen(UiScreen& screen) {
  if (loadFailed) return;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout =
      golfui::chromeLayout(renderer, mappedInput, screen.frame().safeRect(), metrics.topPadding, {}, keepTouchFooter());
  screen.setContentMargin(layout.contentMargins);
  if (phase == Phase::Count) {
    if (mappedInput.hasTouch()) {
      drawTouchCount(screen);
      return;
    }
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
