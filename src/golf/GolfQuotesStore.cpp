#include "GolfQuotesStore.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>
#include <esp_random.h>

#include <cstdio>

#include "GolfQuotes.h"

namespace {

constexpr char QUOTES_PATH[] = "/golf/quotes.txt";

uint32_t quoteRandom(const uint32_t bound) { return esp_random() % bound; }

}  // namespace

bool golfPickRandomQuote(char* quoteOut, const size_t quoteCapacity, char* authorOut, const size_t authorCapacity,
                         bool& hasAuthorOut) {
  if (quoteCapacity > 0) quoteOut[0] = '\0';
  if (authorCapacity > 0) authorOut[0] = '\0';
  hasAuthorOut = false;

  HalFile file;
  if (!Storage.openFileForRead("GOLF", QUOTES_PATH, file)) {
    LOG_DBG("GOLF", "Quote file unavailable");
    return false;
  }

  GolfQuoteReservoir reservoir(quoteRandom);
  char chunk[128];
  while (file.available() > 0) {
    const int bytesRead = file.read(chunk, sizeof(chunk));
    if (bytesRead <= 0) {
      LOG_DBG("GOLF", "Quote file read stopped early");
      return false;
    }
    reservoir.feed(chunk, static_cast<size_t>(bytesRead));
  }
  reservoir.finish();
  if (!reservoir.hasPick()) {
    LOG_DBG("GOLF", "Quote file is empty");
    return false;
  }

  const GolfQuote& selected = reservoir.pick();
  if (quoteCapacity > 0) snprintf(quoteOut, quoteCapacity, "%s", selected.text);
  if (authorCapacity > 0) snprintf(authorOut, authorCapacity, "%s", selected.author);
  hasAuthorOut = selected.hasAuthor && authorCapacity > 0;
  return quoteCapacity > 0;
}

#endif  // CROSSPOINT_GOLF
