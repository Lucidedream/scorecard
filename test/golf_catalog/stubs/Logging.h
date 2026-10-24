#pragma once
// Evaluate log arguments on the host so logging-only values remain checked.
template <typename... Args>
inline void golfTestLog(Args&&...) {}
#define LOG_ERR(...) golfTestLog(__VA_ARGS__)
#define LOG_INF(...) golfTestLog(__VA_ARGS__)
#define LOG_DBG(...) golfTestLog(__VA_ARGS__)
