#pragma once

#include <cstdint>

// Adapted from ricmoo/QRCode v0.0.1 (MIT, license in GolfQrCode.cpp).
// Version 5 / ECC M, at most 84 bytes. Workspace belongs to the activity.
namespace golfqr {
inline constexpr uint8_t SIZE = 37;
struct Code {
  uint8_t modules[172]{};
};
struct Workspace {
  uint8_t codewords[135]{};
  uint8_t correction[135]{};
  uint8_t functions[172]{};
};
bool encode(Code& code, Workspace& workspace, const char* payload);
bool module(const Code& code, uint8_t x, uint8_t y);
}  // namespace golfqr
