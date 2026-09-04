#include "GolfTipListActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfTipNoteActivity.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

void GolfTipListActivity::onEnter() {
  scratch_ = makeUniqueNoThrow<Scratch>();
  if (!scratch_) {
    LOG_ERR("GOLF", "OOM: tip list scratch (%u bytes)", static_cast<unsigned>(sizeof(Scratch)));
    state_ = GolfTipsListState::Error;
    noteCount_ = 0;
  } else {
    loadNotes();
  }
  UiListActivity::onEnter();
}

void GolfTipListActivity::loadNotes() {
  const GolfTipsListResult result = GolfTipsStore::enumerate(scratch_->entries, GOLF_MAX_TIPS);
  noteCount_ = result.count;
  state_ = golfTipsListState(result.directoryError, result.fileError, result.count);
  nav.reset();
}

const char* GolfTipListActivity::headerTitle() const { return tr(STR_GOLF_TIPS); }

void GolfTipListActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (state_ != GolfTipsListState::Ready || index < 0 || index >= noteCount_) return;
  const GolfTipEntry& entry = scratch_->entries[index];
  auto note = makeUniqueNoThrow<GolfTipNoteActivity>(renderer, mappedInput, entry.filename, entry.title);
  if (!note) {
    LOG_ERR("GOLF", "OOM: tip note activity");
    return;
  }
  startActivityForResult(std::move(note), nullptr);
}

void GolfTipListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (state_ == GolfTipsListState::Error) {
    screen.centeredText(tr(STR_GOLF_TIPS_LIST_ERROR), screen.theme().bodyText);
    return;
  }
  if (state_ == GolfTipsListState::Empty) {
    screen.centeredText(tr(STR_GOLF_TIPS_EMPTY), screen.theme().bodyText);
    return;
  }

  // A hint line at the bottom, so the folder location is visible with a note
  // already present too.
  fui::TextStyle hintStyle = screen.theme().smallText;
  const int16_t hintHeight = screen.target().lineHeight(hintStyle.font);
  const int16_t gap = static_cast<int16_t>(metrics.verticalSpacing);
  if (screen.body().height > hintHeight + gap * 3) {
    screen.target().text(screen.takeBottom(hintHeight, gap), tr(STR_GOLF_TIPS_LIST_HINT), hintStyle);
  }

  for (uint8_t index = 0; index < noteCount_; ++index) {
    const GolfTipEntry& entry = scratch_->entries[index];
    fui::ListItem& row = scratch_->rows[index];
    row = {};
    row.label = entry.title;
    const char* format = entry.sectionCount == 1 ? tr(STR_GOLF_TIP_SECTIONS_ONE) : tr(STR_GOLF_TIP_SECTIONS_FORMAT);
    snprintf(scratch_->subtitles[index], sizeof(scratch_->subtitles[index]), format,
             static_cast<unsigned>(entry.sectionCount));
    row.subtitle = scratch_->subtitles[index];
    row.actionValue = static_cast<int16_t>(index);
  }

  listProps_ = {};
  listProps_.items = scratch_->rows;
  listProps_.count = noteCount_;
  listProps_.action = ACTION_ROW;
  listProps_.inputMask = fui::InputTouch;
  syncListViewport(screen, listProps_, /*hasSubtitle=*/true);
  screen.list(listProps_);
}

void GolfTipListActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle());
}

#endif
