#pragma once

// macOS Finder writes a hidden "._<name>" AppleDouble sidecar file next to every
// file it copies onto a non-HFS+ volume (the case for the SD cards this project
// reads and writes), carrying resource-fork/extended-attribute data. The sidecar
// keeps the original extension, so a directory scan that filters purely by
// extension must also reject a leading dot, or the sidecar is listed as a
// phantom entry alongside the real file.
inline bool golfIsHiddenSidecarFilename(const char* filename) { return filename != nullptr && filename[0] == '.'; }
