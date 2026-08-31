#pragma once

// Called from the main loop. Each disconnected -> connected transition starts
// an asynchronous SNTP refresh of the ESP32 system clock.
void golfSystemClockWifiTick(bool connected);

