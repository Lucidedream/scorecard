#pragma once

#include "activities/Activity.h"

class GolfMessageActivity final : public Activity {
 public:
  GolfMessageActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* title, const char* message)
      : Activity("GolfMessage", renderer, mappedInput), title(title), message(message) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  const char* title;
  const char* message;
};
