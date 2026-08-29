#pragma once

class ActivityManager;
class GfxRenderer;
class MappedInputManager;

bool openGolfHome(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool openGolfSetup(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool openGolfScoring(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool resumeGolfRound(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool flushGolfRoundForSleep();
bool flushGolfRoundIfDirty();
void markGolfRoundDirty();
void clearGolfRoundDirty();
bool isGolfRoundDirty();
