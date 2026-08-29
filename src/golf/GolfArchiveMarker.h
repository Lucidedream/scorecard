#pragma once

#include "GolfPaths.h"

struct GolfArchiveMarker {
  char archivedAs[GOLF_ROUND_FILENAME_BUFFER_SIZE];
};

static_assert(sizeof(GolfArchiveMarker) == GOLF_ROUND_FILENAME_BUFFER_SIZE);

bool setGolfArchiveMarker(GolfArchiveMarker& marker, const char* filename);
void clearGolfArchiveMarker(GolfArchiveMarker& marker);
bool isGolfArchiveMarked(const GolfArchiveMarker& marker);
const char* golfArchivedFilename(const GolfArchiveMarker& marker);
