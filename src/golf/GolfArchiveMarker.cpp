#include "GolfArchiveMarker.h"

#if defined(CROSSPOINT_GOLF)

#include <cstring>

bool setGolfArchiveMarker(GolfArchiveMarker& marker, const char* filename) {
  if (filename == nullptr) {
    return false;
  }
  const size_t length = strlen(filename);
  if (length == 0 || length >= sizeof(marker.archivedAs)) {
    return false;
  }
  memcpy(marker.archivedAs, filename, length + 1);
  return true;
}

void clearGolfArchiveMarker(GolfArchiveMarker& marker) { marker.archivedAs[0] = '\0'; }

bool isGolfArchiveMarked(const GolfArchiveMarker& marker) { return marker.archivedAs[0] != '\0'; }

const char* golfArchivedFilename(const GolfArchiveMarker& marker) { return marker.archivedAs; }

#endif
