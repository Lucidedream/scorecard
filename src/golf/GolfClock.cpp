#include "GolfClock.h"

#if defined(CROSSPOINT_GOLF)

#include <Arduino.h>
#include <Logging.h>

#include <ctime>

void golfSystemClockWifiTick(const bool connected) {
  static bool wasConnected = false;
  if (connected && !wasConnected) {
    // configTzTime starts lwIP SNTP and updates the POSIX system clock when a
    // reply arrives. Calendar offset is applied only when a round is archived.
    configTzTime("UTC0", "pool.ntp.org", "time.nist.gov");
    const time_t now = time(nullptr);
    LOG_INF("GOLF", "System clock SNTP refresh started (current epoch %lld)", static_cast<long long>(now));
  }
  wasConnected = connected;
}

#endif
