#pragma once

#include <cstddef>

// Streams /golf/quotes.txt and copies one random record into caller-owned
// fixed buffers. Missing, unreadable, and empty files all return false.
bool golfPickRandomQuote(char* quoteOut, size_t quoteCapacity, char* authorOut, size_t authorCapacity,
                         bool& hasAuthorOut);
