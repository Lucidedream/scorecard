# Authoring a course file

Drop the finished file on the SD card at `/golf/courses/<slug>.json`, either by
removing the card or by uploading through CrossPoint's built-in webserver
(`POST /upload`, or the `/files` page in a browser).

`<slug>` is the course name lowercased with non-alphanumerics collapsed to single
hyphens — `Pebble Beach` becomes `pebble-beach.json`. See CONTRACTS §5.

## Fields

| Field | Required | Notes |
| --- | --- | --- |
| `v` | yes | Schema version. Always `1`. |
| `name` | yes | Shown on the scoring screen. Keep under 40 characters. |
| `tees` | no | Free text — `Blue`, `White`, `Championship`. Under 12 characters. |
| `holes` | yes | `9` or `18`. Nothing else is accepted. |
| `par` | yes | Exactly `holes` entries. |
| `yards` | no | Exactly `holes` entries when present. Shown in the hole band. |
| `si` | no | Stroke index / handicap ranking, 1–18. Currently displayed only. |

Every array must have exactly `holes` entries. A length that disagrees with `holes`
is treated as a corrupt file and rejected — it is never padded or truncated, because
a file that disagrees with itself has no trustworthy reading.

## Where to get the numbers

The par and stroke-index values are printed on the paper scorecard in the pro shop,
and almost every course publishes them on its own website. Yardages depend on which
tees you actually play, so take them from that row of the card.

Par is the only field the app truly needs. Yardage and stroke index are display-only
today, so a course file with just `name`, `holes`, and `par` is perfectly valid and
takes about a minute to write.

## Worked example: Pebble Beach, Blue tees

`pebble-beach.json` in this directory is a real, valid course file — copy it to
`/golf/courses/pebble-beach.json` on the SD card and it works as-is.

It carries `par` only, and that is deliberate rather than lazy.

**Verified.** Pebble Beach plays to **par 72**, front nine 36 and back nine 36, with the
sequence `4,5,4,4,3,5,3,4,4 / 4,4,3,4,5,4,4,3,5`. Note that US Open setups convert the
2nd from a par 5 to a par 4; the file uses the par 5 that the course plays for everyone
else. From the Blue tees the course measures **6,802 yards, rating 74.9, slope 144**.

**Not verified, and therefore omitted.** Hole-by-hole *Blue tee* yardages and stroke
indexes could not be confirmed from a trustworthy source — the course database sites are
CAPTCHA-gated and the official scorecard PDF did not resolve. Rather than mix unverified
numbers into a file whose par values *are* verified, `yards` and `si` are left out. Both
are optional and display-only, so nothing is lost functionally.

If you want them, take that row straight off the paper card in the pro shop and add:

```json
  "yards": [ ... 18 entries, Blue tee row ... ],
  "si":    [ ... 18 entries, stroke index ... ]
```

Each array must have exactly 18 entries or the file is rejected under CONTRACTS §9.

For reference only, the **1992 US Open** setup measured 6,809 yards and ran
`373, 502, 398, 327, 166, 516, 107, 431, 464 / 426, 384, 202, 392, 565, 397, 402, 209, 548`.
That total sits within 7 yards of today's Blue tees, so it is a plausible starting point
— but it is a 30-year-old championship setup, not the current Blue card, so check it
against the real scorecard before trusting any single hole.

## Sanyang Golf & Country Club, Suzhou — a deliberately incomplete file

`sanyang-suzhou.json` is a **template that will not load until you finish it.** That is
intentional, not an oversight.

**Verified.** Sanyang Golf & Country Club (苏州三阳高尔夫乡村俱乐部) at 68 Hubin Rd,
Wuzhong, Suzhou, Jiangsu is an **18-hole, par-72** course, roughly 7,022 yards, rated
74.9, opened 1995. The 18-hole point matters: many Chinese clubs are 27 holes in three
nines, which would change the file's shape entirely.

**Not available.** Hole-by-hole par could not be obtained from any reachable source —
the course-database sites are Cloudflare-gated and the Chinese booking pages do not
publish the card.

**Why the pars are zeros.** A par of `0` fails validation (par must be 3–6), so
`CourseStore` will **skip this file and log why** rather than load it. Had I filled in a
plausible-looking par sequence instead, the file would load silently and your to-par
would be quietly wrong on every hole I guessed incorrectly — for every round you ever
played there. A file that refuses to load is a far better failure than one that lies.

**To finish it:** take the par row off the paper scorecard in the pro shop and replace
the eighteen zeros. Then check it sums to 72:

```bash
python3 -c "import json;p=json.load(open('sanyang-suzhou.json'))['par'];print('holes',len(p),'front',sum(p[:9]),'back',sum(p[9:]),'total',sum(p))"
```

Expect `holes 18` and `total 72`. If the total is not 72, a hole has been mistyped.
Delete the `_TODO` line once done — unknown fields are ignored, but leaving it invites
confusion later.

Add `yards` and `si` the same way if you want them; both are optional and display-only.
