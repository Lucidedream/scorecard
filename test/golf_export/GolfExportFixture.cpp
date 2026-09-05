#include <cstdlib>
#include <cstring>
#include <iostream>

#include "GolfPenalty.h"
#include "GolfRoundExport.h"

int main(int argc, char** argv) {
  if (argc != 3) return 1;
  GolfExportData data{};
  data.round.holeCount = 9;
  strcpy(data.round.courseName, "=Golf, \"山\" <&>");
  auto& player = data.round.players[0];
  strcpy(player.name, "Noah");
  player.tee = TeeSelection::White;
  for (auto& par : data.round.par) par = 4;
  player.score.in100[0] = 3;
  player.score.putts[0] = 2;
  player.score.out100[0] = 2;
  golfAppendPenalty(player.score, 0, GolfField::Out100, GolfPenaltyKind::Ob);
  if (strcmp(argv[2], "summary") == 0) {
    data.detailed = false;
    data.summary.holes = 9;
    data.summary.strokes = 8;
    data.summary.par = 4;
    data.summary.in100 = 3;
    data.summary.out100 = 3;
    data.summary.putts = 2;
    data.summary.obs = 1;
    strcpy(data.summary.course, data.round.courseName);
    strcpy(data.summary.playerName, player.name);
  }
  GolfRoundExport cursor;
  if (!cursor.begin(data, static_cast<GolfExportFormat>(atoi(argv[1])), golfExportTranslate)) return 2;
  char buffer[GolfRoundExport::BLOCK_CAPACITY];
  while (!cursor.done()) {
    size_t length;
    if (!cursor.next(buffer, sizeof(buffer), length)) return 3;
    std::cout.write(buffer, length);
  }
}
