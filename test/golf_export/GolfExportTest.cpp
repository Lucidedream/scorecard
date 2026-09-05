#include <gtest/gtest.h>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <array>
#include <cstring>
#include <string>

#include "GolfExportServer.h"
#include "GolfPenalty.h"
#include "GolfQrCode.h"

namespace {
const char* label(GolfExportLabel value) {
  switch (value) {
    case GolfExportLabel::Title:
      return "Round report";
    case GolfExportLabel::Player:
      return "Player";
    case GolfExportLabel::Score:
      return "Gross";
    case GolfExportLabel::Putts:
      return "Putts";
    case GolfExportLabel::In100:
      return "Inside 100";
    case GolfExportLabel::Unavailable:
      return "Unavailable";
    default:
      return "Label";
  }
}

class GolfExportTest : public testing::Test {
 protected:
  GolfExportData data{};
  std::array<char, GolfRoundExport::BLOCK_CAPACITY> block{};
  void SetUp() override {
    data.round.holeCount = 18;
    strcpy(data.round.courseName, "Gowin");
    strcpy(data.round.players[0].name, "Noah");
    data.round.players[0].tee = TeeSelection::White;
    data.round.dateYmd = (26 << 9) | (9 << 5) | 5;
    for (auto& par : data.round.par) par = 4;
    auto& score = data.round.players[0].score;
    score.in100[0] = 3;
    score.out100[0] = 2;
    score.putts[0] = 2;
    golfAppendPenalty(score, 0, GolfField::Out100, GolfPenaltyKind::Ob);
  }
  std::string render(GolfExportFormat format) {
    GolfRoundExport cursor;
    EXPECT_TRUE(cursor.begin(data, format, golfExportTranslate));
    std::string result;
    for (unsigned iteration = 0; !cursor.done() && iteration < 200; ++iteration) {
      size_t length;
      if (!cursor.next(block.data(), block.size(), length)) {
        ADD_FAILURE() << "block overflow";
        break;
      }
      result.append(block.data(), length);
    }
    EXPECT_TRUE(cursor.done());
    return result;
  }
};

TEST_F(GolfExportTest, JsonUsesFirmwareSemanticsAndPreservesEvents) {
  const auto json = render(GolfExportFormat::Json);
  EXPECT_NE(json.find("\"gross_strokes\":8,"), std::string::npos);
  EXPECT_NE(json.find("\"putts\":2,"), std::string::npos);
  EXPECT_NE(json.find("\"in100\":3,"), std::string::npos);
  EXPECT_NE(json.find("\"out100\":3,"), std::string::npos);
  EXPECT_NE(json.find("\"penalty_events\":[{\"field\":\"out100\",\"kind\":\"ob\"}]"), std::string::npos);
  EXPECT_NE(json.find("\"gross_strokes\":null"), std::string::npos);
  EXPECT_NE(json.find("\"yards\":null"), std::string::npos);
  EXPECT_NE(json.find("\"date\":\"2026-09-05\""), std::string::npos);
  EXPECT_NE(render(GolfExportFormat::Text).find("Gross strokes: 8"), std::string::npos);
}

TEST_F(GolfExportTest, EverySlotExportsOnlySelectedPlayer) {
  for (uint8_t slot = 0; slot < 4; ++slot) {
    data.playerSlot = slot;
    data.round.players[slot].tee = TeeSelection::Blue;
    strcpy(data.round.players[slot].name, "Selected");
    const auto json = render(GolfExportFormat::Json);
    EXPECT_NE(json.find("\"player\":\"Selected\""), std::string::npos);
    EXPECT_EQ(json.find("Noah"), std::string::npos);
  }
}

TEST_F(GolfExportTest, LegacyMissingFieldsStayUnknown) {
  data.penaltiesRecorded = false;
  data.round.dateYmd = 0;
  for (auto& par : data.round.par) par = 0;
  auto json = render(GolfExportFormat::Json);
  EXPECT_NE(json.find("\"date\":null"), std::string::npos);
  EXPECT_NE(json.find("\"to_par\":null"), std::string::npos);
  EXPECT_NE(json.find("\"penalty_strokes\":null"), std::string::npos);
  EXPECT_NE(json.find("\"penalty_events\":null"), std::string::npos);
}

TEST_F(GolfExportTest, SummaryDoesNotInventHoleDetails) {
  data.detailed = false;
  data.summary.holes = 9;
  data.summary.strokes = 45;
  data.summary.par = 36;
  data.summary.in100 = 20;
  data.summary.putts = 15;
  strcpy(data.summary.course, "Archive");
  strcpy(data.summary.playerName, "Selected");
  const auto json = render(GolfExportFormat::Json);
  EXPECT_NE(json.find("\"detail\":\"summary_only\""), std::string::npos);
  EXPECT_NE(json.find("\"holes_entered\":null"), std::string::npos);
  EXPECT_NE(json.find("\"gross_strokes\":45,"), std::string::npos);
  EXPECT_NE(json.find("\"holes\":[\n]}"), std::string::npos);
}

TEST_F(GolfExportTest, EscapesNamesForJsonHtmlAndSpreadsheets) {
  strcpy(data.round.courseName, "=SUM(1,2)\"<script>&");
  EXPECT_NE(render(GolfExportFormat::Html).find("&lt;script&gt;&amp;"), std::string::npos);
  EXPECT_EQ(render(GolfExportFormat::Html).find("<script>"), std::string::npos);
  EXPECT_NE(render(GolfExportFormat::Json).find("\\\"<script>"), std::string::npos);
  EXPECT_NE(render(GolfExportFormat::Csv).find("\"'=SUM(1,2)\"\"<script>&\""), std::string::npos);
}

TEST_F(GolfExportTest, MaximumRoundFitsBlocksAndNeverMutatesSnapshot) {
  memset(data.round.courseName, '&', sizeof(data.round.courseName) - 1);
  memset(data.round.players[0].name, '"', sizeof(data.round.players[0].name) - 1);
  auto& score = data.round.players[0].score;
  score = {};
  for (uint8_t h = 0; h < 18; ++h) {
    score.in100[h] = 90;
    score.out100[h] = 90;
    score.putts[h] = 90;
    for (uint8_t event = 0; event < 8; ++event) golfAppendPenalty(score, h, GolfField::Out100, GolfPenaltyKind::Ob);
  }
  const auto original = data;
  for (auto format : {GolfExportFormat::Text, GolfExportFormat::Csv, GolfExportFormat::Json, GolfExportFormat::Html}) {
    EXPECT_LT(render(format).size(), 65536u);
  }
  EXPECT_EQ(memcmp(&data, &original, sizeof(data)), 0);
}

TEST_F(GolfExportTest, RejectsInvalidSelectionAndSmallOutputBuffer) {
  GolfRoundExport cursor;
  data.playerSlot = 255;
  EXPECT_FALSE(cursor.begin(data, GolfExportFormat::Json, label));
  data.playerSlot = 0;
  data.round.holeCount = 19;
  EXPECT_FALSE(cursor.begin(data, GolfExportFormat::Json, label));
  data.round.holeCount = 18;
  ASSERT_TRUE(cursor.begin(data, GolfExportFormat::Json, label));
  char small[8];
  size_t length;
  EXPECT_FALSE(cursor.next(small, sizeof(small), length));
}

TEST(GolfQrTest, DeterministicAndRejectsOverCapacity) {
  golfqr::Code one{}, two{};
  golfqr::Workspace workspace{};
  ASSERT_TRUE(golfqr::encode(one, workspace, "WIFI:T:WPA;S:Scorecard-1234;P:ABCDEFGH23456789;;"));
  ASSERT_TRUE(golfqr::encode(two, workspace, "WIFI:T:WPA;S:Scorecard-1234;P:ABCDEFGH23456789;;"));
  EXPECT_EQ(memcmp(&one, &two, sizeof(one)), 0);
  EXPECT_TRUE(golfqr::module(one, 0, 0));
  EXPECT_TRUE(golfqr::module(one, 3, 3));
  EXPECT_FALSE(golfqr::module(one, 1, 1));
  EXPECT_FALSE(golfqr::encode(one, workspace, std::string(85, 'a').c_str()));
}

TEST(GolfQrTest, MatchesPinnedRicmooVersionFiveMediumVectors) {
  constexpr const char* payloads[] = {"WIFI:T:WPA;S:Scorecard-1234;P:ABCDEFGH23456789;;", "http://192.168.4.1/"};
  constexpr uint32_t hashes[] = {0xf8c3df1b, 0x8ed8441f};
  golfqr::Code code{};
  golfqr::Workspace workspace{};
  for (unsigned i = 0; i < 2; ++i) {
    ASSERT_TRUE(golfqr::encode(code, workspace, payloads[i]));
    uint32_t hash = 2166136261u;
    for (uint8_t y = 0; y < golfqr::SIZE; ++y)
      for (uint8_t x = 0; x < golfqr::SIZE; ++x) hash = (hash ^ golfqr::module(code, x, y)) * 16777619u;
    EXPECT_EQ(hash, hashes[i]);
  }
}

#if !defined(_WIN32)

class GolfHttpTest : public GolfExportTest {
 protected:
  GolfExportServer server;
  int client = -1;
  void SetUp() override {
    GolfExportTest::SetUp();
    ASSERT_TRUE(server.begin(data, golfExportTranslate, 100, 0));
  }
  void TearDown() override {
    if (client >= 0) close(client);
  }
  void connectClient() {
    client = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(client, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(server.port());
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
  }
  std::string request(const char* content) {
    connectClient();
    EXPECT_EQ(send(client, content, strlen(content), 0), static_cast<ssize_t>(strlen(content)));
    std::string result;
    for (unsigned i = 0; i < 100000; ++i) {
      server.poll(100 + i / 100);
      const int count = recv(client, block.data(), block.size(), MSG_DONTWAIT);
      if (count > 0) result.append(block.data(), count);
      if (count == 0) return result;
    }
    ADD_FAILURE() << "response did not finish";
    return result;
  }
};

TEST_F(GolfHttpTest, ServesExactDownloadWithLengthAndNoMutation) {
  auto response = request("GET /round.json HTTP/1.1\r\nHost: x4\r\n\r\n");
  ASSERT_EQ(response.find("HTTP/1.1 200 OK"), 0u);
  auto split = response.find("\r\n\r\n");
  ASSERT_NE(split, std::string::npos);
  const auto body = response.substr(split + 4);
  EXPECT_EQ(body, render(GolfExportFormat::Json));
  EXPECT_NE(response.find("Content-Length: " + std::to_string(body.size())), std::string::npos);
  EXPECT_NE(response.find("Content-Disposition: attachment; filename=\"scorecard-2026-09-05-gowin-p1.json\""),
            std::string::npos);
  EXPECT_EQ(server.downloadsServed(), 1u);
}

TEST_F(GolfHttpTest, RejectsFilesystemPaths) {
  EXPECT_EQ(request("GET /../golf/state.json HTTP/1.1\r\nHost: x4\r\n\r\n").find("HTTP/1.1 404"), 0u);
  EXPECT_EQ(server.lastActivity(), 100u);
}
TEST_F(GolfHttpTest, RejectsWrites) {
  EXPECT_EQ(request("POST /round.json HTTP/1.1\r\nHost: x4\r\n\r\n").find("HTTP/1.1 405"), 0u);
}
TEST_F(GolfHttpTest, BoundsOversizedHeaders) {
  const std::string oversized = "GET / HTTP/1.1\r\nX-Padding: " + std::string(2000, 'x');
  EXPECT_EQ(request(oversized.c_str()).find("HTTP/1.1 431"), 0u);
  EXPECT_EQ(server.lastActivity(), 100u);
}
TEST_F(GolfHttpTest, StalledClientExpiresAndServerCanRestart) {
  connectClient();
  for (unsigned i = 0; i < 10000 && !server.clientActive(); ++i) server.poll(100);
  ASSERT_TRUE(server.clientActive());
  server.poll(15100);
  pollfd descriptor{client, POLLIN, 0};
  ASSERT_GT(poll(&descriptor, 1, 1000), 0);
  char byte;
  EXPECT_EQ(recv(client, &byte, 1, MSG_DONTWAIT), 0);
  EXPECT_EQ(server.lastActivity(), 100u);
  server.stop();
  EXPECT_FALSE(server.running());
  EXPECT_TRUE(server.begin(data, label, 20000, 0));
}
#endif
}  // namespace
