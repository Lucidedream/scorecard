#pragma once

#include "GolfUiLayout.h"
#include "activities/UiListActivity.h"

class GolfUiListActivity : public UiListActivity {
 protected:
  GolfUiListActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput,
                     bool wantsTouchLongPress = false)
      : UiListActivity(name, renderer, mappedInput, wantsTouchLongPress) {}

  bool handleButtons() override;
  void drawFooter() override;
  virtual bool keepTouchFooter() const { return false; }
  virtual golfui::FooterLabels golfFooterLabels() const;
  virtual void onFooterConfirm();
  virtual void onFooterPrevious();
  virtual void onFooterNext();
};
