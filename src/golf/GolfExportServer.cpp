#include "GolfExportServer.h"

#if defined(CROSSPOINT_GOLF)

#ifdef ARDUINO
#include <Logging.h>
#include <lwip/sockets.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#define LOG_ERR(...) ((void)0)
#endif

#include <fcntl.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "GolfPaths.h"

namespace {
bool wouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
bool nonblocking(int socket) { return fcntl(socket, F_SETFL, O_NONBLOCK) == 0; }
}  // namespace

GolfExportServer::~GolfExportServer() { stop(); }

uint16_t GolfExportServer::port() const {
  sockaddr_in address{};
  socklen_t length = sizeof(address);
  return listener >= 0 && getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0
             ? ntohs(address.sin_port)
             : 0;
}

bool GolfExportServer::begin(const GolfExportData& value, GolfExportTranslate labels, uint32_t now, uint16_t port) {
  stop();
  data = &value;
  translate = labels;
  lastMeaningful = now;
  opened = downloads = 0;
  if (!cursor.begin(value, GolfExportFormat::Text, labels)) {
    LOG_ERR("GOLFEXP", "Invalid export snapshot");
    return false;
  }
  listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  int reuse = 1;
  if (listener < 0 || setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0 ||
      !nonblocking(listener) || bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
      listen(listener, 1) != 0) {
    LOG_ERR("GOLFEXP", "HTTP startup failed: %d", errno);
    stop();
    return false;
  }
  return true;
}

void GolfExportServer::closeClient() {
  if (client >= 0) close(client);
  client = -1;
  requestLength = outputLength = outputSent = 0;
  responding = document = download = false;
}

void GolfExportServer::stop() {
  closeClient();
  if (listener >= 0) close(listener);
  listener = -1;
}

void GolfExportServer::error(const char* status) {
  document = download = false;
  responding = true;
  outputSent = 0;
  outputLength =
      snprintf(output, sizeof(output),
               "HTTP/1.1 %s\r\nConnection: close\r\nContent-Length: 0\r\nCache-Control: no-store\r\n\r\n", status);
}

bool GolfExportServer::prepare(uint32_t now) {
  const char* lineEnd = strstr(request, "\r\n");
  if (!lineEnd || strncmp(request, "GET ", 4) != 0) {
    error("405 Method Not Allowed");
    return false;
  }
  char* path = request + 4;
  char* end = strchr(path, ' ');
  if (!end || end > lineEnd || (strncmp(end, " HTTP/1.1\r\n", 11) != 0 && strncmp(end, " HTTP/1.0\r\n", 11) != 0)) {
    error("400 Bad Request");
    return false;
  }
  *end = 0;
  GolfExportFormat format = GolfExportFormat::Html;
  download = true;
  if (strcmp(path, "/") == 0)
    download = false;
  else if (strcmp(path, "/round.txt") == 0)
    format = GolfExportFormat::Text;
  else if (strcmp(path, "/round.csv") == 0)
    format = GolfExportFormat::Csv;
  else if (strcmp(path, "/round.json") == 0)
    format = GolfExportFormat::Json;
  else if (strcmp(path, "/round.html") != 0) {
    error("404 Not Found");
    return false;
  }

  // Count the immutable stream before sending headers, so a failed formatter
  // never publishes a successful, truncated download.
  size_t total = 0, length = 0;
  if (!cursor.begin(*data, format, translate)) {
    error("500 Internal Server Error");
    return false;
  }
  while (!cursor.done()) {
    if (!cursor.next(output, sizeof(output), length) || total + length > 65536) {
      LOG_ERR("GOLFEXP", "Export exceeds bounded response");
      error("500 Internal Server Error");
      return false;
    }
    total += length;
  }
  cursor.begin(*data, format, translate);
  char date[GOLF_DATE_BUFFER_SIZE]{};
  if (!golfFormatDate(data->detailed ? data->round.dateYmd : data->summary.dateYmd, date, sizeof(date))) {
    snprintf(date, sizeof(date), "undated");
  }
  char slug[GOLF_SLUG_BUFFER_SIZE]{};
  golfSlug(data->detailed ? data->round.courseName : data->summary.course, slug, sizeof(slug));
  outputLength = snprintf(
      output, sizeof(output),
      "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\n"
      "X-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\n"
      "Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; frame-ancestors "
      "'none'\r\n",
      GolfRoundExport::mimeType(format), static_cast<unsigned>(total));
  if (download)
    outputLength += snprintf(output + outputLength, sizeof(output) - outputLength,
                             "Content-Disposition: attachment; filename=\"scorecard-%s-%s-p%u.%s\"\r\n", date, slug,
                             static_cast<unsigned>(data->playerSlot + 1), GolfRoundExport::extension(format));
  outputLength += snprintf(output + outputLength, sizeof(output) - outputLength, "\r\n");
  outputSent = 0;
  responding = document = true;
  lastMeaningful = now;
  if (!download) ++opened;
  return true;
}

void GolfExportServer::poll(uint32_t now) {
  if (listener < 0) return;
  if (client < 0) {
    client = accept(listener, nullptr, nullptr);
    if (client < 0) return;
    if (!nonblocking(client)) {
      closeClient();
      return;
    }
#ifdef SO_NOSIGPIPE
    int noSignal = 1;
    setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
#endif
    connectedAt = now;
  }
  if (now - connectedAt >= CLIENT_DEADLINE_MS) {
    closeClient();
    return;
  }
  if (!responding) {
    const int bytes = recv(client, request + requestLength, sizeof(request) - requestLength - 1, 0);
    if (bytes <= 0) {
      if (bytes == 0 || !wouldBlock()) closeClient();
      return;
    }
    // Embedded NULs must not hide a different request from the parser.
    if (memchr(request + requestLength, 0, bytes)) {
      error("400 Bad Request");
      return;
    }
    requestLength += bytes;
    request[requestLength] = 0;
    if (strstr(request, "\r\n\r\n"))
      prepare(now);
    else if (requestLength == sizeof(request) - 1)
      error("431 Request Header Fields Too Large");
    return;
  }
  if (outputSent < outputLength) {
    const int bytes = send(client, output + outputSent, outputLength - outputSent,
#ifdef MSG_NOSIGNAL
                           MSG_NOSIGNAL
#else
                           0
#endif
    );
    if (bytes > 0) {
      outputSent += bytes;
      if (document) lastMeaningful = now;
    } else if (bytes == 0 || !wouldBlock())
      closeClient();
    return;
  }
  if (!document || cursor.done()) {
    if (document && download) ++downloads;
    closeClient();
    return;
  }
  if (!cursor.next(output, sizeof(output), outputLength)) {
    LOG_ERR("GOLFEXP", "Export generation failed during response");
    closeClient();
    return;
  }
  outputSent = 0;
}

#endif
