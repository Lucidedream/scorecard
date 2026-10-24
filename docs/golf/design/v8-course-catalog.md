# v8 design: a course catalog from baigolf

Status: **decided** 2026-10-24 (see §9). The binding contract is CONTRACTS-V2 §41;
where this document and §41 differ, §41 wins.

## 1. Goal

Grow Course Map from 4 hand-made courses to the hundreds baigolf.com lists, and make
picking one fast with buttons (X4) and touch (X4 Pro):

- pick by **region** (province, then city);
- a **Recent** section for courses already played;
- show each course's **baigolf rating**;
- keep hole maps, tee info and the nine picker working for every catalog course.

## 2. What baigolf offers (checked 2026-10-24)

| Source | What it gives | Notes |
|---|---|---|
| `sitemap.php` (one page) | 3,955 courses grouped as country → province | China: **442 courses in 31 provinces**; Japan 1,828; Korea 405 |
| `api.php?_mod=BaiGolfOrder&_cmd=GET_GOLF_COURSE_DETAIL` (POST `golf_course_id`) | names (zh/en/short), address, lat/lng, province/city ids, holes, par, yardage, slope, designer, year opened, **rating** `eval_score_avg` + review count, category scores, **hole-image list** | works without the captcha params the page sends |
| `api.php?_mod=BaiGolfCourse&_cmd=getScorecards` (POST `golf_course_id`) | **one table per nine**: Par, HDCP (SI), one row per tee | maps 1:1 onto our v2 nines |
| `ball_path_img_list` image URLs | one PNG per hole, 206×400 colour, BaiGolf watermark | titles are messy: `1..18`, `B1..B9`, full-width `Ｃ1..Ｃ9`, plus a hash-named overview |

Verified on your courses: Sanyang (533), MoganShan (589), Nine Dragons (611), Jianshan
Dragon (605) and Phoenix (623) all exist. Nine Dragons comes back as OCEAN / LAKE /
QUARRY with par and Black/Blue/Red yardage **identical** to your hand-entered file. Quarry
uses **Yellow** where the other nines use White, which is why White had no Quarry data.

Rating fields: `eval_score_avg` (e.g. 4.2) is the 5-point user rating from
`total_eval.peop_total` reviews. The USGA-style `rating` is empty for
the courses sampled, and `slope` (100–105) does not look like a USGA slope, so neither is
shown.

**Terms and licence.** robots.txt allows everything. baigolf's terms (`about.php?op=clause`
§5) reserve copyright on text and images and allow copying only for **non-commercial,
personal** use, with the copyright notice kept, and forbid mirroring or republishing.
`Lucidedream/scorecard` is a **public** repo. So:

- the scraper code and tests with small synthetic fixtures go in the repo;
- scraped data and images **never** do. They go to a gitignored local cache and onto your
  own SD cards only;
- the watermark stays on the images, and the catalog footer credits "Data: baigolf.com";
- requests are slow and polite: one request at a time with ≥2 s spacing, a resumable
  cache so each URL is fetched once, and an honest User-Agent.

## 3. Architecture

```
 Mac (host)                                    SD card
 ─────────────                                 ───────────────────────────────
 scripts/baigolf_scrape.py  ──► cache/         /golf/courses/          (yours, unchanged)
   sitemap → detail → scorecards → images      /golf/catalog/regions.txt
 scripts/baigolf_export.py  ──────────────────►/golf/catalog/index.dat
   cache → v2 course JSON + index + BMPs       /golf/catalog/courses/bg-<id>.json
                                               /golf/maps/bg-<id>/hole-N.bmp
```

Two host steps keep slow network work apart from fast, repeatable export:

1. **Scrape** (network, resumable): writes raw API JSON and PNGs to
   `~/golf-catalog-cache/` (outside the repo). A re-run only fetches what's missing;
   `--refresh-ratings` re-fetches detail only.
2. **Export** (offline, deterministic): turns the cache into SD files. It validates
   every course with the same §40 rules the firmware uses, and skips with a report rather
   than writing a bad file.

The firmware never talks to baigolf. It only reads SD files.

## 4. Data mapping (export step)

**Course file** `/golf/catalog/courses/bg-<id>.json`, standard v2 (§40) plus two
optional fields the parser already tolerates as unknown:

```json
{ "v": 2, "name": "苏州三阳高尔夫乡村俱乐部",
  "maps": "bg-533",
  "baigolf": { "id": 533, "rating": 42, "reviews": 37 },
  "nines": [ { "name": "Out", "par": [...], "si": [...] }, { "name": "In", ... } ],
  "tees":  [ { "name": "Black", "yards": [[...],[...]] }, ... ] }
```

- **Nines.** One per scorecard table, in API order. Names are normalised: `OUT`→Out,
  `IN`→In, `OCEAN COURSE`→Ocean, `A Course`→A. 1–4 nines of exactly 9 holes, otherwise
  the course is skipped and reported.
- **SI.** From the HDCP row, only when it passes §40's rules; otherwise omitted (Nine
  Dragons has none).
- **Tees.** Every row except Hole/Par/HDCP/N-D. A tee with yardage on **every** nine keeps
  it; a tee missing on some nine (Yellow/White at Nine Dragons) becomes yardless, matching
  your earlier choice. At most `GOLF_MAX_TEES` (6), longest first.
- **Name.** Must be unique and ≤39 bytes (the `courseName[40]` used by rounds and
  History). See decision D2.
- **`maps`.** New optional field naming the map folder. Today the folder comes from
  `golfSlug(name)`, which strips non-ASCII; every Chinese name would collapse to the
  shared `course` folder. Missing `maps` falls back to `golfSlug(name)`, so your current
  files keep working.
- **Hole images.** Sorted into course hole order using the title's trailing number within
  each nine group (`1..9`, `B1..B9`, `Ｃ1..Ｃ9` after full-width→ASCII). The overview image
  is dropped. A course whose titles don't resolve to exactly `9 × nineCount` holes gets
  no maps rather than wrong maps. Converted to 8-bit grayscale BMP and **pre-scaled to
  fit the map body**, because §26's renderer only scales down.

**Catalog index** (the device must never hold hundreds of summaries in RAM;
`GOLF_MAX_COURSES` is 32 and the C3 has about 148 KB free):

- `/golf/catalog/regions.txt`: one line per region, `code|name|count|firstRecord`, sorted
  by name. About 31 lines for China, so the whole file fits in RAM (≈2 KB).
- `/golf/catalog/index.dat`: **fixed-width 128-byte records**, sorted region → city →
  name. Each holds id, region code, city name, display name, nine count, par, rating ×10,
  review count and a has-maps flag. Showing page *k* of a region is one `seek` plus 10
  reads, with no scanning and no per-course JSON parse. Records are padded UTF-8 text, so
  the file is readable in a text editor.

## 5. On-device UI

All screens are `GolfUiListActivity` lists, so they work with buttons and the touch
footer. Only one page of rows is held in memory.

```
Course Map
 ├─ Recent                  last 5 distinct courses from /golf/rounds/index.csv
 ├─ My courses              /golf/courses + built-ins (today's list)
 └─ By region  ─►  Provinces (31, with counts)
                    └─►  Courses in 浙江  (paged; sort: Rating ▸ Name)
                          └─►  Course info
                                 ├─ Hole maps  (existing browser)
                                 ├─ Tee info   (existing page)
                                 └─ Play this course  (→ nine picker → players)
```

- **Course row:** line 1 is the name; line 2 is `★4.2 (37) · 27 holes · 平湖`. Courses
  with fewer than 5 reviews show the rating dimmed. A course with no maps is marked so
  you don't open an empty browser.
- **Province list:** name and course count. Front Left/Right (or swipe) jumps a page.
  The confirm-cell long-press toggles the sort between Rating and Name.
- **Course info** (new screen): name, city/address, rating with review count, the six
  category scores as short bars (greens, difficulty, caddies, value, clubhouse, food),
  holes/par/yardage, designer and year. Three actions: Hole maps, Tee info, Play.
- **Recent** reads the History index already on the card; it needs no new file. A recent
  course is resolved by name in My courses first, then the catalog.
- **New round** uses the same picker (Recent / My courses / By region), so the hundreds of
  courses are also playable, not just browsable. See decision D5.

Chinese text renders through the existing UI→SD-font fallback (`SdCardFontSystem`). Both
devices now use SourceHanSerif, which covers CJK. With a Latin-only card font, Chinese
names would show as missing glyphs. See decision D2.

## 6. Your courses vs catalog courses

Your files in `/golf/courses` stay as they are, appear under **My courses**, and are never
overwritten. Old rounds keep their course names. The export reports likely duplicates
(e.g. your "Nine Dragons Hill" vs catalog 611) so you can choose per course:

- **keep yours**, adding `"baigolf": {"id": 611}` so it gains the rating and region
  listing; or
- **retire yours** and play the catalog copy from now on.

## 7. Size on the card (China, all courses)

| Item | Estimate |
|---|---|
| Course JSON + index | < 2 MB |
| Hole maps, 8-bit, pre-scaled ≈330×640 | ~210 KB × ~9,500 holes ≈ **2 GB** |
| Hole maps, native 206×400 (would display small) | ≈ 0.8 GB |
| First scrape, polite pacing | detail + scorecards ≈ 30 min; images ≈ 6 h (resumable) |

Both cards have about 14 GB free. If 2 GB of maps is too much, images can be exported per
province (`--maps 浙江,江苏,上海`). A later firmware change could store maps as 2-bit BMP,
which the 4-level grey panel can show anyway, for about 4× smaller files. That change is
not in this plan.

## 8. Delivery phases

| Phase | Work | Firmware change |
|---|---|---|
| P1 | Scraper + exporter + tests (synthetic fixtures); `.gitignore` the cache; a host check that every exported course passes §40 parsing | none |
| P2 | `maps` field; catalog index reader; Recent / My courses / By region lists; Course info screen; rating display | yes |
| P3 | Same picker in New round; "Play this course"; duplicate linking (`baigolf.id`) | yes |

P1 is useful on its own: run it, check the data on the Mac, and copy a few courses to the
card as plain v2 files before any firmware work.

## 9. Decisions (owner, 2026-10-24)

D1 China only. D2 English names (shortened per §41.3). D3 maps for every course,
exported as 2-bit BMP by default (~0.5 GB; the reader supports 2 bpp natively).
D4 rating with review count, plus category scores on Course info. D5 the same picker
in New round.

Original questions:

- **D1 Scope:** China only (442), or more countries? Japan alone is 1,828 and would push
  the image download to about 1 day.
- **D2 Names:** Chinese full name (e.g. 苏州三阳高尔夫乡村俱乐部, 36 B) where it fits 39
  bytes, else the short name (九龙山将军)? English (`Jiangsu Sanyang Golf &Country
  Club`)? Or Chinese with English as the second line?
- **D3 Maps:** all 442 courses (~2 GB), or only chosen provinces?
- **D4 Rating:** show the 5-point user rating with review count (recommended), and the
  six category scores on Course info?
- **D5 New round:** use the same region/recent picker there too (recommended), or Course
  Map only?
