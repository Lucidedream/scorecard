#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Utf8.h>
#include <Xtc.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

#if CROSSPOINT_GOLF
#include "activities/golf/GolfNavigation.h"
#include "activities/golf/GolfStrings.h"

namespace {

// v3.2 (CONTRACTS-V2.md §14): Scorecard is its own row at the very top of home,
// above the recent-book cover tile, and is selected on entry. It takes the
// first selector slot; every recent-book tile and menu row below it is the
// upstream home laid out kHomeSelectorShift slots lower. loop() and render()
// both map a selector slot to its upstream position through
// homeUpstreamSelector() — a result below zero is the Scorecard row — so two
// sites can never resolve the same slot to different things. The earlier
// off-by-one bugs in this file came from exactly that kind of duplicated
// position arithmetic.
constexpr int kHomeSelectorShift = 1;

int homeUpstreamSelector(int selectorIndex) { return selectorIndex - kHomeSelectorShift; }

}  // namespace
#endif

int HomeActivity::getMenuItemCount() const {
  int count = 4;  // File Browser, Recents, File transfer, Settings
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsServers) {
    count++;
  }
#if CROSSPOINT_GOLF
  count++;  // Scorecard row (selector slot 0), above the cover tile
#endif
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (RecentBooksStore::isMissing(book)) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!Storage.exists(coverPath.c_str())) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.crosspoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = epub.generateThumbBmp(coverHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(coverHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            coverRendered = false;
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  hasOpdsServers = OPDS_STORE.hasServers();

  const auto& metrics = UITheme::getInstance().getMetrics();
  loadRecentBooks(metrics.homeRecentBooksCount);

  const auto base = static_cast<int>(recentBooks.size());
  selectorIndex = initialMenuItem == HomeMenuItem::NONE ? 0 : base + menuItemToIndex(initialMenuItem, hasOpdsServers);
#if CROSSPOINT_GOLF
  // Default (NONE) lands on selector slot 0 — the Scorecard row, selected on
  // entry (CONTRACTS-V2.md §14). A targeted re-entry still wins: shift its
  // upstream index past the Scorecard slot.
  if (initialMenuItem != HomeMenuItem::NONE) selectorIndex += kHomeSelectorShift;
#endif

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  // render() must have already set the cover rect; without it we'd be back to
  // cloning the whole framebuffer.
  if (coverRectW <= 0 || coverRectH <= 0) return false;
  freeCoverBuffer();
  const size_t needed = renderer.getRegionByteSize(coverRectX, coverRectY, coverRectW, coverRectH);
  if (needed == 0) return false;
  coverBuffer = static_cast<uint8_t*>(malloc(needed));
  if (!coverBuffer) {
    LOG_ERR("HOME", "OOM: cover buffer (%u bytes)", (unsigned)needed);
    return false;
  }
  coverBufferSize = needed;
  if (!renderer.copyRegionToBuffer(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize)) {
    free(coverBuffer);
    coverBuffer = nullptr;
    coverBufferSize = 0;
    return false;
  }
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer || coverRectW <= 0 || coverRectH <= 0) return false;
  return renderer.copyBufferToRegion(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize);
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    free(coverBuffer);
    coverBuffer = nullptr;
  }
  coverBufferSize = 0;
  coverBufferStored = false;
}

void HomeActivity::loop() {
  const int menuCount = getMenuItemCount();
  const auto& metrics = UITheme::getInstance().getMetrics();

#if CROSSPOINT_GOLF
  auto activateSelection = [this] {
    const int upstreamSelector = homeUpstreamSelector(selectorIndex);
    if (upstreamSelector < 0) {
      // Scorecard has no HomeMenuItem value; adding one would touch a further
      // upstream file (ActivityManager.h).
      openGolfHome(activityManager, renderer, mappedInput);
      return;
    }
#else
  auto activateSelection = [this] {
    const int upstreamSelector = selectorIndex;
#endif
    if (upstreamSelector < recentBooks.size()) {
      onSelectBook(recentBooks[upstreamSelector].path);
      return;
    }
    const int menuIndex = upstreamSelector - static_cast<int>(recentBooks.size());
    switch (indexToMenuItem(menuIndex, hasOpdsServers)) {
      case HomeMenuItem::FILE_BROWSER:
        onFileBrowserOpen();
        break;
      case HomeMenuItem::RECENTS:
        onRecentsOpen();
        break;
      case HomeMenuItem::OPDS_BROWSER:
        onOpdsBrowserOpen();
        break;
      case HomeMenuItem::FILE_TRANSFER:
        onFileTransferOpen();
        break;
      case HomeMenuItem::SETTINGS_MENU:
        onSettingsOpen();
        break;
      default:
        break;
    }
  };

  buttonNavigator.onNext([this, menuCount] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, menuCount] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up) {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Down) {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }

  // Back is otherwise unused on the home menu: open the most recently read
  // book directly (recentBooks is most-recent-first and already pruned of
  // files missing from the SD card).
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && !recentBooks.empty()) {
    onSelectBook(recentBooks[0].path);
    return;
  }

#if CROSSPOINT_GOLF
  // v3.2 (CONTRACTS-V2.md §14): the Scorecard row occupies the band between the
  // header and the cover tile; the tile and the menu below it shift down by its
  // height. Same geometry the render() pass uses.
  const int scorecardBandHeight = metrics.verticalSpacing + GUI.getMenuRowHeight(renderer) + metrics.menuSpacing;
  const int coverTop = metrics.homeTopPadding + scorecardBandHeight;
  {
    int scorecardRow = -1;
    const auto scorecardTouch = mappedInput.rowTouch(scorecardRow, metrics.homeTopPadding, scorecardBandHeight, 1, 0,
                                                     INT32_MAX, scorecardBandHeight);
    if (scorecardTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != 0) {  // slot 0 is the Scorecard row
        selectorIndex = 0;
        requestUpdate();
      }
      return;
    }
    if (scorecardTouch == MappedInputManager::RowTouch::Tap) {
      selectorIndex = 0;
      activateSelection();
      return;
    }
  }
#endif
  const int coverColumnCount = std::max(1, metrics.homeRecentBooksCount);
  const int recentCount = std::min(static_cast<int>(recentBooks.size()), coverColumnCount);
  const int coverColumnWidth = (renderer.getScreenWidth() - 2 * metrics.contentSidePadding) / coverColumnCount;
  int touchedBook = -1;
#if CROSSPOINT_GOLF
  const auto coverTouch = mappedInput.colTouch(touchedBook, metrics.contentSidePadding, coverColumnWidth, recentCount,
                                               coverTop, coverTop + metrics.homeCoverTileHeight, coverColumnWidth);
#else
  const auto coverTouch = mappedInput.colTouch(touchedBook, metrics.contentSidePadding, coverColumnWidth, recentCount,
                                               metrics.homeTopPadding,
                                               metrics.homeTopPadding + metrics.homeCoverTileHeight, coverColumnWidth);
#endif
  if (coverTouch != MappedInputManager::RowTouch::None) {
#if CROSSPOINT_GOLF
    const int bookSlot = touchedBook + kHomeSelectorShift;
    if (coverTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != bookSlot) {
        selectorIndex = bookSlot;
        requestUpdate();
      }
    } else {
      selectorIndex = bookSlot;
      activateSelection();
    }
#else
    if (coverTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedBook) {
        selectorIndex = touchedBook;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedBook;
      activateSelection();
    }
#endif
    return;
  }

#if CROSSPOINT_GOLF
  const int menuTop = coverTop + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset;
  const int renderedMenuCount = menuCount -
                                (metrics.homeContinueReadingInMenu ? 0 : static_cast<int>(recentBooks.size())) -
                                kHomeSelectorShift;  // Scorecard left the menu list for its own row
#else
  const int menuTop = metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset;
  const int renderedMenuSelection =
      metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size();
  const int renderedMenuCount =
      menuCount - (metrics.homeContinueReadingInMenu ? 0 : static_cast<int>(recentBooks.size()));
#endif
  int menuRow = -1;
  // Row height from the theme, not the metrics table: RoundedRaff draws
  // font-derived rows and the touch grid must match the visuals exactly.
  const int menuRowHeight = GUI.getMenuRowHeight(renderer);
  const auto menuTouch = mappedInput.rowTouch(menuRow, menuTop, menuRowHeight + metrics.menuSpacing, renderedMenuCount,
                                              0, INT32_MAX, menuRowHeight);
  if (menuTouch != MappedInputManager::RowTouch::None) {
#if CROSSPOINT_GOLF
    const int touchedIndex =
        (metrics.homeContinueReadingInMenu ? menuRow : menuRow + static_cast<int>(recentBooks.size())) +
        kHomeSelectorShift;  // past the Scorecard slot
#else
    const int touchedIndex =
        metrics.homeContinueReadingInMenu ? menuRow : menuRow + static_cast<int>(recentBooks.size());
#endif
    if (menuTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedIndex) {
        selectorIndex = touchedIndex;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedIndex;
      activateSelection();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelection();
  }
}

void HomeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

#if CROSSPOINT_GOLF
  const int upstreamSelector = homeUpstreamSelector(selectorIndex);
  const int scorecardBandHeight = metrics.verticalSpacing + GUI.getMenuRowHeight(renderer) + metrics.menuSpacing;
  const int coverTop = metrics.homeTopPadding + scorecardBandHeight;
#endif

  renderer.clearScreen();
  bool bufferRestored = coverBufferStored && restoreCoverBuffer();

  // Band spans topPadding..homeTopPadding: the cover tile starts at the fixed
  // homeTopPadding, so the height must shrink by topPadding or the band (and a
  // centered title, e.g. RoundedRaff's book title) sinks into the tile.
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding - metrics.topPadding},
                 metrics.homeContinueReadingInMenu && !recentBooks.empty() ? recentBooks[0].title.c_str() : nullptr);

  // Record the tile rect so storeCoverBuffer (called from the theme) knows
  // which sub-region of the framebuffer to snapshot. ~16 KB in Portrait
  // instead of the 48 KB full framebuffer the previous bind captured.
  coverRectX = 0;
#if CROSSPOINT_GOLF
  coverRectY = coverTop;
#else
  coverRectY = metrics.homeTopPadding;
#endif
  coverRectW = pageWidth;
  coverRectH = metrics.homeCoverTileHeight;

#if CROSSPOINT_GOLF
  GUI.drawRecentBookCover(renderer, Rect{0, coverTop, pageWidth, metrics.homeCoverTileHeight}, recentBooks,
                          upstreamSelector, coverRendered, coverBufferStored, bufferRestored,
                          std::bind(&HomeActivity::storeCoverBuffer, this));
#else
  GUI.drawRecentBookCover(renderer, Rect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight},
                          recentBooks, selectorIndex, coverRendered, coverBufferStored, bufferRestored,
                          std::bind(&HomeActivity::storeCoverBuffer, this));
#endif

#if CROSSPOINT_GOLF
  // v3.2 (CONTRACTS-V2.md §14): Scorecard is its own row in the band above the
  // cover tile, highlighted when the selector is on it (upstream index < 0).
  GUI.drawButtonMenu(
      renderer, Rect{0, metrics.homeTopPadding, pageWidth, scorecardBandHeight}, 1, upstreamSelector < 0 ? 0 : -1,
      [](int) { return std::string(GolfStrings::APP_TITLE); }, [](int) { return Scorecard; });
#endif

  // Build menu items dynamically
  std::vector<const char*> menuItems;
  std::vector<UIIcon> menuIcons;
  menuItems.reserve(7);
  menuIcons.reserve(7);
  menuItems.insert(menuItems.end(),
                   {tr(STR_BROWSE_FILES), tr(STR_MENU_RECENT_BOOKS), tr(STR_FILE_TRANSFER), tr(STR_SETTINGS_TITLE)});
  menuIcons.insert(menuIcons.end(), {Folder, Recent, Transfer, Settings});

  if (hasOpdsServers) {
    menuItems.insert(menuItems.begin() + 2, tr(STR_OPDS_BROWSER));
    menuIcons.insert(menuIcons.begin() + 2, Library);
  }

  if (metrics.homeContinueReadingInMenu && !recentBooks.empty()) {
    // Insert Continue Reading at the top if enabled in theme
    menuItems.insert(menuItems.begin(), tr(STR_CONTINUE_READING));
    menuIcons.insert(menuIcons.begin(), Book);
  }

#if CROSSPOINT_GOLF
  GUI.drawButtonMenu(
      renderer,
      Rect{0, coverTop + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset, pageWidth,
           pageHeight - (metrics.headerHeight + coverTop + metrics.verticalSpacing + metrics.homeMenuTopOffset +
                         metrics.buttonHintsHeight)},
      static_cast<int>(menuItems.size()),
      metrics.homeContinueReadingInMenu ? upstreamSelector : upstreamSelector - recentBooks.size(),
      [&menuItems](int index) { return std::string(menuItems[index]); },
      [&menuIcons](int index) { return menuIcons[index]; });
#else
  GUI.drawButtonMenu(
      renderer,
      Rect{0, metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset, pageWidth,
           pageHeight - (metrics.headerHeight + metrics.homeTopPadding + metrics.verticalSpacing +
                         metrics.homeMenuTopOffset + metrics.buttonHintsHeight)},
      static_cast<int>(menuItems.size()),
      metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size(),
      [&menuItems](int index) { return std::string(menuItems[index]); },
      [&menuIcons](int index) { return menuIcons[index]; });
#endif

  const auto labels = mappedInput.mapLabels(recentBooks.empty() ? "" : tr(STR_RESUME), tr(STR_SELECT), tr(STR_DIR_UP),
                                            tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);

  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(metrics.homeCoverHeight);
  }
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onRecentsOpen() { activityManager.goToRecentBooks(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }
