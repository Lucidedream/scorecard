#pragma once

#include <cstdint>

namespace golfmenu {
enum Row : uint8_t { VIEW_CARD, HOLE_MAP, ABANDON, FINISH, SEND, TIPS, ROW_COUNT };

constexpr Row rowAt(int index) {
  return index >= 0 && index < ROW_COUNT ? static_cast<Row>(index) : ROW_COUNT;
}
}  // namespace golfmenu
