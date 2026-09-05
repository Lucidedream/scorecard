#include "GolfRoundExport.h"

#if defined(CROSSPOINT_GOLF)

#include <cstdio>
#include <cstring>

#include "GolfPaths.h"
#include "GolfPenalty.h"
#include "GolfStats.h"

namespace {

using Label = GolfExportLabel;
using Format = GolfExportFormat;
constexpr uint8_t METADATA_COUNT = 10;
constexpr uint8_t METRIC_COUNT = 15;

class Writer {
 public:
  char* output;
  size_t capacity;
  size_t used = 0;
  bool ok = true;
  Format format;

  void raw(const char* value) {
    const size_t length = strlen(value);
    if (used + length >= capacity) {
      ok = false;
      return;
    }
    memcpy(output + used, value, length);
    used += length;
    output[used] = '\0';
  }
  void number(int value, bool available = true) {
    if (!available) {
      raw(format == Format::Json ? "null" : "");
      return;
    }
    char text[16];
    snprintf(text, sizeof(text), "%d", value);
    raw(text);
  }
  void text(const char* value) {
    if (format == Format::Json || format == Format::Csv) raw("\"");
    // Spreadsheet programs may evaluate a quoted cell as a formula.
    if (format == Format::Csv && (*value == '=' || *value == '+' || *value == '-' || *value == '@' || *value == '\t' ||
                                  *value == '\r' || *value == '\n'))
      raw("'");
    for (const auto* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
      if (format == Format::Html && *p == '&')
        raw("&amp;");
      else if (format == Format::Html && *p == '<')
        raw("&lt;");
      else if (format == Format::Html && *p == '>')
        raw("&gt;");
      else if (format == Format::Html && *p == '"')
        raw("&quot;");
      else if (format == Format::Json && (*p == '"' || *p == '\\')) {
        char escaped[] = {'\\', static_cast<char>(*p), 0};
        raw(escaped);
      } else if (format == Format::Json && *p < 32) {
        char escaped[7];
        snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
        raw(escaped);
      } else if (format == Format::Csv && *p == '"')
        raw("\"\"");
      else {
        char c[] = {static_cast<char>(*p), 0};
        raw(c);
      }
    }
    if (format == Format::Json || format == Format::Csv) raw("\"");
  }
  void field(const char* key, const char* label, const char* value, bool available = true) {
    if (format == Format::Json) {
      text(key);
      raw(":");
      if (available)
        text(value);
      else
        raw("null");
      raw(",\n");
    } else if (format == Format::Html) {
      raw("<div class=field><dt>");
      text(label);
      raw("</dt><dd>");
      text(value);
      raw("</dd></div>\n");
    } else {
      text(label);
      raw(": ");
      text(value);
      raw("\n");
    }
  }
  void metric(const char* key, const char* label, int value, bool available, const char* missing) {
    if (format == Format::Json) {
      text(key);
      raw(":");
      number(value, available);
      raw(",\n");
    } else {
      char textValue[16];
      snprintf(textValue, sizeof(textValue), "%d", value);
      field(key, label, available ? textValue : missing);
    }
  }
};

const GolfPlayer& player(const GolfExportData& d) { return d.round.players[d.playerSlot]; }
const char* course(const GolfExportData& d) { return d.detailed ? d.round.courseName : d.summary.course; }
const char* name(const GolfExportData& d) { return d.detailed ? player(d).name : d.summary.playerName; }
uint8_t holes(const GolfExportData& d) { return d.detailed ? d.round.holeCount : d.summary.holes; }
bool hasPar(const GolfExportData& d) { return d.detailed ? golfHasPar(d.round) : d.summary.par != 0; }
bool entered(const GolfExportData& d, uint8_t hole) {
  return player(d).score.in100[hole] + player(d).score.out100[hole] != 0;
}

constexpr const char* METRIC_KEYS[] = {"gross_strokes",
                                       "played_par",
                                       "to_par",
                                       "putts",
                                       "in100",
                                       "out100",
                                       "short_excluding_putts",
                                       "penalty_strokes",
                                       "hazards",
                                       "out_of_bounds",
                                       "holes_entered",
                                       "hole_count",
                                       "one_putt_holes",
                                       "three_plus_putt_holes",
                                       "recovered"};
constexpr Label METRIC_LABELS[] = {Label::Score,  Label::Par,   Label::ToPar,    Label::Putts,      Label::In100,
                                   Label::Out100, Label::Short, Label::Penalty,  Label::Hazards,    Label::Obs,
                                   Label::Thru,   Label::Holes, Label::OnePutts, Label::ThreePutts, Label::Recovered};

int metric(const GolfExportData& d, uint8_t index, bool& available) {
  const auto& r = d.round;
  const auto& s = player(d).score;
  const auto& h = d.summary;
  available = true;
  if (index == 1 || index == 2) available = hasPar(d);
  if (index >= 7 && index <= 9) available = d.penaltiesRecorded;
  if (index == 10 || index == 12 || index == 13) available = d.detailed;
  switch (index) {
    case 0:
      return d.detailed ? golfScore(r, s) : h.strokes;
    case 1:
      return d.detailed ? golfParTotal(r, s) : h.par;
    case 2:
      return d.detailed ? golfToPar(r, s) : static_cast<int>(h.strokes) - h.par;
    case 3:
      return d.detailed ? golfPuttsTotal(r, s) : h.putts;
    case 4:
      return d.detailed ? golfIn100Total(r, s) : h.in100;
    case 5:
      return d.detailed ? golfLongTotal(r, s) : h.out100;
    case 6:
      return d.detailed ? golfShortTotal(r, s) : static_cast<int>(h.in100) - h.putts;
    case 7:
      return d.detailed ? golfPenaltyTotal(r, s) : h.hazards + 2 * h.obs;
    case 8:
      return d.detailed ? golfHazardsForRound(s, r.holeCount) : h.hazards;
    case 9:
      return d.detailed ? golfObsForRound(s, r.holeCount) : h.obs;
    case 10:
      return golfThru(r, s);
    case 11:
      return holes(d);
    case 12:
      return golfOnePutts(r, s);
    case 13:
      return golfThreePutts(r, s);
    default:
      return d.repaired ? 1 : 0;
  }
}

void metadata(Writer& w, const GolfExportData& d, GolfExportTranslate tr, uint8_t index) {
  char date[GOLF_DATE_BUFFER_SIZE]{};
  const bool dated = golfFormatDate(d.detailed ? d.round.dateYmd : d.summary.dateYmd, date, sizeof(date));
  switch (index) {
    case 0:
      w.field("course", tr(Label::Course), course(d));
      break;
    case 1:
      w.field("player", tr(Label::Player), name(d));
      break;
    case 2:
      w.metric("player_slot", tr(Label::Slot), d.playerSlot + 1, true, "");
      break;
    case 3:
      w.field("date", tr(Label::Date), dated ? date : tr(Label::Unavailable), dated);
      break;
    case 4:
      w.field("tee", tr(Label::Tee),
              d.detailed
                  ? (w.format == Format::Json ? (player(d).tee == TeeSelection::White ? "White" : "Blue")
                                              : tr(player(d).tee == TeeSelection::White ? Label::White : Label::Blue))
                  : tr(Label::Unavailable),
              d.detailed);
      break;
    case 5:
      w.field("status", tr(Label::Status),
              w.format == Format::Json ? (d.archived ? "archived" : "in_progress")
                                       : tr(d.archived ? Label::Archived : Label::InProgress));
      break;
    case 6:
      w.field("detail", tr(Label::Detail),
              w.format == Format::Json ? (d.detailed ? "full" : "summary_only")
                                       : tr(d.detailed ? Label::Yes : Label::SummaryOnly));
      break;
    case 7:
      w.field("penalties_recorded", tr(Label::Events),
              w.format == Format::Json ? (d.penaltiesRecorded ? "recorded" : "unavailable")
                                       : tr(d.penaltiesRecorded ? Label::Yes : Label::Unavailable));
      break;
    case 8:
      w.field("distance_unit", tr(Label::Yards), w.format == Format::Json ? "yards" : tr(Label::Yards));
      break;
    case 9:
      if (w.format != Format::Json) {
        w.raw(w.format == Format::Html ? "</dl><p>" : "\n");
        w.text(tr(Label::Dictionary));
        w.raw(w.format == Format::Html ? "</p><dl>" : "\n");
      }
      break;
  }
}

void events(Writer& w, const GolfExportData& d, uint8_t hole, GolfExportTranslate tr) {
  const auto& s = player(d).score;
  if (w.format == Format::Json) w.raw("[");
  for (uint8_t i = 0; i < s.penaltyCount[hole]; ++i) {
    GolfPenaltyEvent event{};
    if (!golfPenaltyEventAt(s, hole, i, event)) continue;
    if (i) w.raw(w.format == Format::Json ? "," : "; ");
    constexpr const char* FIELDS[] = {"putts", "in100", "out100"};
    constexpr Label LABELS[] = {Label::Putts, Label::In100, Label::Out100};
    if (w.format == Format::Json) {
      w.raw("{\"field\":");
      w.text(FIELDS[static_cast<uint8_t>(event.field)]);
      w.raw(",\"kind\":");
      w.text(event.kind == GolfPenaltyKind::Hazard ? "hazard" : "ob");
      w.raw("}");
    } else {
      w.text(tr(LABELS[static_cast<uint8_t>(event.field)]));
      w.raw("/");
      w.text(tr(event.kind == GolfPenaltyKind::Hazard ? Label::Hazards : Label::Obs));
    }
  }
  if (w.format == Format::Json) w.raw("]");
}

void holeBlock(Writer& w, const GolfExportData& d, uint8_t hole, GolfExportTranslate tr) {
  const auto& r = d.round;
  const auto& p = player(d);
  const auto& s = p.score;
  const bool played = entered(d, hole);
  if (w.format == Format::Json)
    w.raw(hole == 0 ? "{" : ",{\n");
  else if (w.format == Format::Html)
    w.raw("</dl><section><h2>");
  else
    w.raw("\n");
  if (w.format != Format::Json) {
    w.text(tr(Label::Hole));
    w.raw(" ");
    w.number(hole + 1);
    w.raw(w.format == Format::Html ? "</h2><dl>" : "\n");
  } else {
    w.raw("\"hole\":");
    w.number(hole + 1);
    w.raw(",");
  }
  const char* missing = tr(Label::Unavailable);
  w.metric("entered", tr(Label::Entered), played, true, missing);
  w.metric("par", tr(Label::Par), r.par[hole], r.par[hole] >= 3 && r.par[hole] <= 6, missing);
  w.metric("si", tr(Label::Si), r.si[hole], r.hasSi && r.si[hole] > 0, missing);
  w.metric("yards", tr(Label::Yards), p.yards[hole], p.yards[hole] != 0, missing);
  w.metric("gross_strokes", tr(Label::Score), golfHoleScore(r, s, hole), played, missing);
  w.metric("putts", tr(Label::Putts), s.putts[hole], played, missing);
  w.metric("in100", tr(Label::In100), s.in100[hole], played, missing);
  w.metric("out100", tr(Label::Out100), s.out100[hole], played, missing);
  w.metric("penalty_strokes", tr(Label::Penalty), golfPenaltyStrokesForHole(s, hole), d.penaltiesRecorded && played,
           missing);
  if (w.format == Format::Json)
    w.raw("\"penalty_events\":");
  else {
    w.raw(w.format == Format::Html ? "<div class=field><dt>" : "");
    w.text(tr(Label::Events));
    w.raw(w.format == Format::Html ? "</dt><dd>" : ": ");
  }
  if (d.penaltiesRecorded)
    events(w, d, hole, tr);
  else if (w.format == Format::Json)
    w.raw("null");
  else
    w.text(missing);
  if (w.format == Format::Json)
    w.raw("}\n");
  else
    w.raw(w.format == Format::Html ? "</dd></div></dl></section><dl>" : "\n");
}

void csvBlock(Writer& w, const GolfExportData& d, uint16_t block, GolfExportTranslate tr) {
  if (block == 0) {
    w.raw(
        "course,player,player_slot,date,status,detail,hole,entered,par,si,yards,putts,in100,out100,gross_strokes,"
        "penalty_strokes,penalty_events,penalties_recorded,recovered\r\n");
    return;
  }
  char date[GOLF_DATE_BUFFER_SIZE]{};
  golfFormatDate(d.detailed ? d.round.dateYmd : d.summary.dateYmd, date, sizeof(date));
  w.text(course(d));
  w.raw(",");
  w.text(name(d));
  w.raw(",");
  w.number(d.playerSlot + 1);
  w.raw(",");
  w.text(date);
  w.raw(",");
  w.text(d.archived ? "archived" : "in_progress");
  w.raw(",");
  w.text(d.detailed ? "full" : "summary_only");
  w.raw(",");
  if (d.detailed) {
    const auto hole = static_cast<uint8_t>(block - 1);
    const auto& r = d.round;
    const auto& p = player(d);
    const auto& s = p.score;
    const bool played = entered(d, hole);
    w.number(hole + 1);
    w.raw(",");
    w.number(played);
    w.raw(",");
    w.number(r.par[hole], r.par[hole] >= 3 && r.par[hole] <= 6);
    w.raw(",");
    w.number(r.si[hole], r.hasSi && r.si[hole] != 0);
    w.raw(",");
    w.number(p.yards[hole], p.yards[hole] != 0);
    w.raw(",");
    w.number(s.putts[hole], played);
    w.raw(",");
    w.number(s.in100[hole], played);
    w.raw(",");
    w.number(s.out100[hole], played);
    w.raw(",");
    w.number(golfHoleScore(r, s, hole), played);
    w.raw(",");
    w.number(golfPenaltyStrokesForHole(s, hole), played && d.penaltiesRecorded);
    w.raw(",\"");
    // CSV event tokens are stable machine fields, independent of UI language.
    for (uint8_t i = 0; d.penaltiesRecorded && i < s.penaltyCount[hole]; ++i) {
      GolfPenaltyEvent e{};
      if (!golfPenaltyEventAt(s, hole, i, e)) continue;
      if (i) w.raw(";");
      w.number(static_cast<uint8_t>(e.field));
      w.raw(":");
      w.number(static_cast<uint8_t>(e.kind));
    }
    w.raw("\",");
  } else {
    w.raw(",,");
    w.number(d.summary.par, hasPar(d));
    w.raw(",,,");
    w.number(d.summary.putts);
    w.raw(",");
    w.number(d.summary.in100);
    w.raw(",");
    w.number(d.summary.out100);
    w.raw(",");
    w.number(d.summary.strokes);
    w.raw(",");
    w.number(d.summary.hazards + 2 * d.summary.obs, d.penaltiesRecorded);
    w.raw(",,");
  }
  w.number(d.penaltiesRecorded);
  w.raw(",");
  w.number(d.repaired);
  w.raw("\r\n");
  (void)tr;
}

void segment(Writer& w, const GolfExportData& d, uint8_t start, GolfExportTranslate tr) {
  uint16_t gross = 0, par = 0;
  for (uint8_t h = start; h < holes(d) && h < start + 9; ++h) {
    if (!entered(d, h)) continue;
    gross += golfHoleScore(d.round, player(d).score, h);
    par += d.round.par[h];
  }
  w.metric(start == 0 ? "front_gross" : "back_gross", tr(start == 0 ? Label::Front : Label::Back), gross,
           d.detailed && start < holes(d), tr(Label::Unavailable));
  if (w.format == Format::Json) {
    w.metric(start == 0 ? "front_played_par" : "back_played_par", "", par, d.detailed && start < holes(d) && hasPar(d),
             "");
  }
}

}  // namespace

bool GolfRoundExport::begin(const GolfExportData& value, const Format outputFormat, GolfExportTranslate labels) {
  data = &value;
  format = outputFormat;
  translate = labels;
  block = 0;
  finished = true;
  if (!labels || value.playerSlot >= GolfRound::MAX_PLAYERS || (holes(value) != 9 && holes(value) != 18)) return false;
  if (value.detailed && !golfPlayerIsEnabled(player(value))) return false;
  if (!memchr(course(value), 0, 40) || !memchr(name(value), 0, GolfPlayer::NAME_CAPACITY)) return false;
  if (value.detailed) {
    const auto& selected = player(value);
    if (selected.tee != TeeSelection::White && selected.tee != TeeSelection::Blue) return false;
    for (uint8_t h = 0; h < holes(value); ++h) {
      const auto& score = selected.score;
      if (score.putts[h] > score.in100[h] || score.in100[h] > 99 || score.out100[h] > 99 ||
          score.penaltyCount[h] > GOLF_MAX_PENALTIES_PER_HOLE)
        return false;
      for (uint8_t i = 0; i < score.penaltyCount[h]; ++i) {
        GolfPenaltyEvent event{};
        if (!golfPenaltyEventAt(score, h, i, event)) return false;
      }
    }
  }
  finished = false;
  return true;
}

bool GolfRoundExport::next(char* output, size_t capacity, size_t& written) {
  written = 0;
  if (!output || capacity == 0 || !data) return false;
  output[0] = '\0';
  if (finished) return true;
  Writer w{output, capacity, 0, true, format};
  const auto& d = *data;
  if (format == Format::Csv) {
    csvBlock(w, d, block, translate);
    finished = block >= (d.detailed ? holes(d) : 1);
  } else if (block == 0) {
    if (format == Format::Json)
      w.raw("{\"schema\":\"scorecard.round-export\",\"version\":1,\"scoring_rules_version\":1,\n");
    else if (format == Format::Html) {
      w.raw(
          "<!doctype html><html><head><meta charset=utf-8><meta name=viewport "
          "content=\"width=device-width,initial-scale=1\"><title>");
      w.text(translate(Label::Title));
      w.raw(
          "</title><style>body{font:17px system-ui;margin:24px auto;padding:0 "
          "16px;max-width:620px;color:#172b24;background:#f8faf8}h1{font-size:28px}h2{font-size:21px}nav{display:grid;"
          "gap:10px}a{padding:12px;background:#173f32;color:white;border-radius:8px;text-decoration:none}.field{"
          "display:flex;gap:16px;justify-content:space-between;padding:5px 0;border-bottom:1px solid "
          "#dce5de}dt{flex:1}dd{margin:0;flex:1;text-align:right;overflow-wrap:anywhere}section{break-inside:avoid}p{"
          "line-height:1.5}@media print{nav,.help{display:none}}</style></head><body><h1>");
      w.text(translate(Label::Title));
      w.raw("</h1><nav>");
    } else {
      w.text(translate(Label::Title));
      w.raw("\n\n");
    }
  } else if (block == 1) {
    if (format == Format::Html) {
      constexpr const char* ROUTES[] = {"/round.txt", "/round.csv", "/round.json", "/round.html"};
      constexpr Label LABELS[] = {Label::DownloadAgent, Label::DownloadCsv, Label::DownloadJson, Label::DownloadHtml};
      for (uint8_t i = 0; i < 4; ++i) {
        w.raw("<a download href=\"");
        w.raw(ROUTES[i]);
        w.raw("\">");
        w.text(translate(LABELS[i]));
        w.raw("</a>");
      }
      w.raw("</nav><p class=help>");
      w.text(translate(Label::DownloadHelp));
      w.raw("</p><p class=help>");
      w.text(translate(Label::SendHelp));
      w.raw("</p><dl>");
    }
  } else if (block < 2 + METADATA_COUNT)
    metadata(w, d, translate, block - 2);
  else if (block < 2 + METADATA_COUNT + METRIC_COUNT) {
    const auto index = static_cast<uint8_t>(block - 2 - METADATA_COUNT);
    bool available;
    const int value = metric(d, index, available);
    w.metric(METRIC_KEYS[index], translate(METRIC_LABELS[index]), value, available, translate(Label::Unavailable));
  } else if (block < 4 + METADATA_COUNT + METRIC_COUNT) {
    segment(w, d, (block - 2 - METADATA_COUNT - METRIC_COUNT) * 9, translate);
  } else if (block == 4 + METADATA_COUNT + METRIC_COUNT) {
    GolfWorstHole worst[3]{};
    const uint8_t count = d.detailed ? golfWorstHoles(d.round, player(d).score, worst, 3) : 0;
    if (format == Format::Json)
      w.raw("\"worst_holes\":[");
    else {
      if (format == Format::Html) w.raw("<div class=field><dt>");
      w.text(translate(Label::Worst));
      w.raw(format == Format::Html ? "</dt><dd>" : ": ");
    }
    for (uint8_t i = 0; i < count; ++i) {
      if (i) w.raw(", ");
      if (format == Format::Json) w.raw("{\"hole\":");
      w.number(worst[i].hole + 1);
      if (format == Format::Json) {
        w.raw(",\"to_par\":");
        w.number(worst[i].toPar);
        w.raw("}");
      } else {
        w.raw(" (");
        w.text(translate(Label::ToPar));
        w.raw(": ");
        w.number(worst[i].toPar);
        w.raw(")");
      }
    }
    if (format == Format::Json)
      w.raw("],\"holes\":[\n");
    else if (format == Format::Html)
      w.raw("</dd></div>");
    else
      w.raw("\n");
  } else {
    const uint16_t hole = block - 5 - METADATA_COUNT - METRIC_COUNT;
    if (d.detailed && hole < holes(d))
      holeBlock(w, d, hole, translate);
    else {
      if (format == Format::Json)
        w.raw("]}\n");
      else if (format == Format::Html)
        w.raw("</dl></body></html>\n");
      finished = true;
    }
  }
  if (!w.ok) return false;
  written = w.used;
  ++block;
  return true;
}

const char* GolfRoundExport::extension(Format format) {
  switch (format) {
    case Format::Text:
      return "txt";
    case Format::Csv:
      return "csv";
    case Format::Json:
      return "json";
    default:
      return "html";
  }
}
const char* GolfRoundExport::mimeType(Format format) {
  switch (format) {
    case Format::Text:
      return "text/plain; charset=utf-8";
    case Format::Csv:
      return "text/csv; charset=utf-8";
    case Format::Json:
      return "application/json";
    default:
      return "text/html; charset=utf-8";
  }
}

#endif
