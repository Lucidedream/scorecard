#include "GolfTipNoteActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <string>
#include <vector>

#include "GolfNavigation.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

// Body text is the largest in the app: read outdoors, at arm's length, possibly
// with gloves (CONTRACTS-V2 §25.2). NOTOSANS_18 is the theme's biggest prose
// face (51 px line box vs UI_12's 29 px everywhere else). The mock's 19 px sits
// between NOTOSANS_16 and _18; _18 wins for legibility and the leading is
// tightened so the densest section still fits one 800 px screen. The heading is
// the same face in bold rather than shrinking the body.
constexpr int HEADING_FONT = NOTOSANS_18_FONT_ID;
constexpr int BODY_FONT = NOTOSANS_18_FONT_ID;
constexpr int SMALL_FONT = UI_10_FONT_ID;
constexpr float BODY_LEADING = 0.80f;
constexpr float HEADING_LEADING = 0.92f;
constexpr char BULLET_GLYPH[] = "\xE2\x80\xA2";  // U+2022, present in the device fonts

constexpr int HEADING_MAX_LINES = 3;
constexpr int BULLET_MAX_LINES = 5;
constexpr int BULLET_INDENT = 26;
constexpr int BULLET_GAP = 10;

}  // namespace

GolfTipNoteActivity::GolfTipNoteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* filename,
                                         const char* title)
    : Activity("GolfTipNote", renderer, mappedInput) {
  snprintf(filename_, sizeof(filename_), "%s", filename != nullptr ? filename : "");
  snprintf(title_, sizeof(title_), "%s", title != nullptr ? title : "");
}

void GolfTipNoteActivity::onEnter() {
  Activity::onEnter();
  section_ = makeUniqueNoThrow<GolfTipSection>();
  if (!section_) {
    LOG_ERR("GOLF", "OOM: tip section (%u bytes)", static_cast<unsigned>(sizeof(GolfTipSection)));
    loadError_ = true;
  } else {
    loadSection(0);
  }
  requestUpdate();
}

void GolfTipNoteActivity::loadSection(const uint16_t index) {
  if (!section_) return;
  loadError_ = !GolfTipsStore::readSection(filename_, index, *section_);
  if (loadError_) return;
  current_ = index;
  sectionCount_ = section_->count;
}

void GolfTipNoteActivity::turnPage(const int delta) {
  if (loadError_ || sectionCount_ == 0) return;
  const uint16_t next = static_cast<uint16_t>((current_ + sectionCount_ + delta) % sectionCount_);
  if (next == current_) return;

  // Never hold a RenderLock across SD I/O: read into a fresh section off-lock,
  // then swap it in under the lock.
  auto staging = makeUniqueNoThrow<GolfTipSection>();
  if (!staging) {
    LOG_ERR("GOLF", "OOM: tip page turn");
    return;
  }
  const bool readOk = GolfTipsStore::readSection(filename_, next, *staging);
  {
    RenderLock lock(*this);
    if (!readOk) {
      loadError_ = true;
    } else {
      section_ = std::move(staging);
      current_ = next;
      sectionCount_ = section_->count;
    }
  }
  requestUpdate();
}

void GolfTipNoteActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const bool swapped = mappedInput.isNavDirectionSwapped();
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    turnPage(golfFrontNavDelta(swapped, true));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    turnPage(golfFrontNavDelta(swapped, false));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    turnPage(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) turnPage(1);
}

void GolfTipNoteActivity::drawSection() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding, freeink::ui::Insets{10, 20, 8, 20});
  const int left = layout.body.x;
  const int width = layout.body.width;
  const int bottom = layout.body.y + layout.body.height;

  const int headingLh = renderer.getLineHeight(HEADING_FONT, HEADING_LEADING);
  const int bodyLh = renderer.getLineHeight(BODY_FONT, BODY_LEADING);
  const int smallLh = renderer.getLineHeight(SMALL_FONT);

  const GolfTipSection& section = *section_;
  const int textWidth = width - BULLET_INDENT;

  // --- heading band --------------------------------------------------------
  const auto headingLines =
      renderer.wrappedText(HEADING_FONT, section.heading, width, HEADING_MAX_LINES, EpdFontFamily::BOLD);
  int y = layout.body.y;
  for (const auto& line : headingLines) {
    renderer.drawText(HEADING_FONT, left, y, line.c_str(), true, EpdFontFamily::BOLD);
    y += headingLh;
  }
  y += 6;
  renderer.drawLine(left, y, left + width, y, 2, true);
  const int bulletTop = y + 14;

  // --- position counter pinned to the bottom ----------------------------
  char counter[24];
  snprintf(counter, sizeof(counter), tr(STR_GOLF_TIP_POSITION_FORMAT), static_cast<unsigned>(current_ + 1),
           static_cast<unsigned>(sectionCount_));
  const int counterY = bottom - smallLh;
  renderer.drawLine(left, counterY - 8, left + width, counterY - 8, 1, true);
  renderer.drawCenteredText(SMALL_FONT, counterY, counter, true, EpdFontFamily::BOLD);
  const int bulletBottom = counterY - 14;
  const int markerHeight = smallLh + 10;

  // --- measure the whole section, so the marker is only shown when a line
  //     genuinely will not fit (a bullet is never split across a break) ----
  int needed = 0;
  uint8_t fits = 0;
  for (uint8_t index = 0; index < section.lineCount; ++index) {
    const bool bullet = section.lines[index].kind == GolfTipLineKind::Bullet;
    const int lineWidth = bullet ? textWidth : width;
    const auto wrapped = renderer.wrappedText(BODY_FONT, section.lines[index].text, lineWidth, BULLET_MAX_LINES);
    const int block = static_cast<int>(wrapped.size()) * bodyLh + (index == 0 ? 0 : BULLET_GAP);
    if (bulletTop + needed + block <= bulletBottom) {
      needed += block;
      ++fits;
    } else {
      break;
    }
  }
  const bool overflow = section.overflow || fits < section.lineCount;
  const int usableBottom = overflow ? bulletBottom - markerHeight : bulletBottom;

  // --- draw the lines that fit ----------------------------------------
  y = bulletTop;
  for (uint8_t index = 0; index < section.lineCount; ++index) {
    const GolfTipSectionLine& line = section.lines[index];
    const bool bullet = line.kind == GolfTipLineKind::Bullet;
    const int lineLeft = bullet ? left + BULLET_INDENT : left;
    const int lineWidth = bullet ? textWidth : width;
    const auto wrapped = renderer.wrappedText(BODY_FONT, line.text, lineWidth, BULLET_MAX_LINES);
    const int gap = index == 0 ? 0 : BULLET_GAP;
    if (y + gap + static_cast<int>(wrapped.size()) * bodyLh > usableBottom) break;
    y += gap;
    if (bullet) renderer.drawText(BODY_FONT, left, y, BULLET_GLYPH, true);
    int lineY = y;
    for (const auto& part : wrapped) {
      renderer.drawText(BODY_FONT, lineLeft, lineY, part.c_str(), true);
      lineY += bodyLh;
    }
    y = lineY;
  }

  if (overflow) {
    const int markerTop = bulletBottom - markerHeight;
    renderer.fillRect(left, markerTop, width, markerHeight, true);
    renderer.drawCenteredText(SMALL_FONT, markerTop + 5, tr(STR_GOLF_TIP_SECTION_TRIMMED), false, EpdFontFamily::BOLD);
  }
}

void GolfTipNoteActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  renderer.clearScreen();
  golfui::drawHeader(renderer, layout.header, title_);

  if (loadError_) {
    const int lh = renderer.getLineHeight(UI_12_FONT_ID);
    UITheme::drawCenteredText(renderer, Rect{layout.body.x, layout.body.y, layout.body.width, layout.body.height},
                              UI_12_FONT_ID, layout.body.y + (layout.body.height - lh) / 2,
                              tr(STR_GOLF_TIP_READ_ERROR));
  } else if (!section_ || sectionCount_ == 0 || !section_->found) {
    const int lh = renderer.getLineHeight(UI_12_FONT_ID);
    UITheme::drawCenteredText(renderer, Rect{layout.body.x, layout.body.y, layout.body.width, layout.body.height},
                              UI_12_FONT_ID, layout.body.y + (layout.body.height - lh) / 2,
                              tr(STR_GOLF_TIP_EMPTY_NOTE));
  } else {
    drawSection();
  }

  const bool paged = !loadError_ && section_ && sectionCount_ > 1 && section_->found;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", paged ? tr(STR_GOLF_BUTTON_PREVIOUS) : "",
                                            paged ? tr(STR_GOLF_BUTTON_NEXT) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

#endif
