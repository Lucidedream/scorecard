#include "GolfUiListActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>

golfui::FooterLabels GolfUiListActivity::golfFooterLabels() const {
  return {tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN)};
}

bool GolfUiListActivity::handleButtons() {
  golfui::FooterInput input(mappedInput, golfFooterLabels(), keepTouchFooter());
  if (input.wasReleased(golfui::FooterAction::Back)) {
    onBackButton();
    return true;
  }
  if (input.wasReleased(golfui::FooterAction::Confirm)) {
    onFooterConfirm();
    return true;
  }
  if (input.wasTouchReleased(golfui::FooterAction::Previous)) {
    onFooterPrevious();
    return true;
  }
  if (input.wasTouchReleased(golfui::FooterAction::Next)) {
    onFooterNext();
    return true;
  }
  return false;
}

void GolfUiListActivity::onFooterConfirm() {
  const int selected = activeNav().selected;
  if (selected >= 0 && selected < listCount()) activateIndex(selected);
}

void GolfUiListActivity::onFooterPrevious() {
  moveSelectionTo(ButtonNavigator::previousIndex(activeNav().selected, listCount()));
}

void GolfUiListActivity::onFooterNext() {
  moveSelectionTo(ButtonNavigator::nextIndex(activeNav().selected, listCount()));
}

void GolfUiListActivity::drawFooter() {
  golfui::drawFooter(renderer, mappedInput, golfFooterLabels(), keepTouchFooter());
}

#endif
