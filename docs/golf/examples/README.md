# Authoring a course file

Put one v2 JSON file per course on the SD card at `/golf/courses/<slug>.json`,
either by removing the card or uploading through CrossPoint's built-in webserver
(`POST /upload`, or the `/files` page in a browser). The filename is not significant
beyond its `.json` extension. For a conventional slug, lowercase the course name,
collapse non-ASCII-alphanumerics to single hyphens, trim surrounding hyphens, and
truncate to 40 characters (CONTRACTS §5).

A course contains one to four named nines, in course order, and up to six tees.
Par and stroke index belong to each nine; a tee's yardage has one row per nine.
For a round, choose its front and back nine from the course. See CONTRACTS-V2 §40.1.

## Fields

| Field | Required | Rules |
| --- | --- | --- |
| `v` | yes | Always `2`; v1 files must be converted. |
| `name` | yes | 1–39 UTF-8 bytes, single line. |
| `nines` | yes | 1–4 entries, in course order; index 0 is the course's front nine. |
| `nines[].name` | yes | 1–11 UTF-8 bytes, single line, no commas; unique within the file, case-sensitive. |
| `nines[].par` | yes | Exactly 9 integers, each 3–6. Zero pars are invalid in SD files. |
| `nines[].si` | no | Exactly 9 distinct integers within each nine: 1–9 for a one-nine course, otherwise 1–18. Repetition across different nines is allowed. |
| `tees` | no | 0–6 entries. Absent or empty synthesizes yardless Blue and White choices. |
| `tees[].name` | yes, per tee | 1–11 UTF-8 bytes, single line, no commas; unique within the file. Free-form names such as `Gold` are accepted. |
| `tees[].yards` | no | Exactly one array per nine in the same order, each with exactly 9 integers in 1–999. Omit for a selection-only tee. |

Unknown fields are ignored. An invalid field or array length rejects the entire
course, with a log naming the file and reason; values are never padded or truncated.
The firmware displays tees longest-first by their total yardage across all nines.

## Example shape

This illustrates a nine-hole course with one yardless tee:

```json
{
  "v": 2,
  "name": "Example Nine",
  "nines": [
    {
      "name": "Out",
      "par": [4, 5, 4, 4, 3, 5, 3, 4, 4]
    }
  ],
  "tees": [
    { "name": "Blue" }
  ]
}
```

For 18 holes, add an `In` nine. For yardages, add `yards` to each tee, with one
nine-entry array for `Out` and another for `In`. Take par, SI, and each tee's
hole-by-hole yardages directly from the paper scorecard or an authoritative course
scorecard. Missing optional data stays unavailable rather than guessed; SI does
not enable handicap or net scoring.

## Included files

* `example-course.json` is an illustrative all-fields example named **Example Course**;
  its yardages and SI are not verified course data.
* `pebble-beach.json` contains the verified everyday par-72 sequence (36 Out, 36 In)
  with a yardless Blue tee. Hole 2 is a par 5; US Open setups may make it a par 4.
  Hole-by-hole Blue yardages and SI remain omitted because they were not confirmed.
* `sanyang-golf-club.json` contains Sanyang's par, SI, and four tees.
* `lake-hill-golf-dragon-course.json` and `lake-hill-golf-phoenix-course.json` remain
  separate courses, each with five tees.
* `moganshan-gowin.json` contains two nines and the Blue tee.

## Converting your old files

Copy the SD card's `/golf/courses` directory to a local `courses-v1` directory,
keeping that copy as a backup. From the repository root run with Python 3.9 or newer:

```bash
python3 scripts/golf_convert_courses_v2.py courses-v1 courses-v2
```

Review the printed summaries and every skip or error before proceeding. The script
merges v1 files with exactly the same course name only when their `holes`, `par`,
and `si` agree (including whether SI is present). It splits 18 holes into `Out` and
`In`, or 9 holes into `Out`, and preserves tee rows in input filename order. A file
without a tee label contributes no tee. Already-v2 files are copied byte-for-byte
under their original filenames. Invalid v1 files are skipped with a reason;
conflicting groups are reported as errors and are not written.

The output directory must differ from the input directory. Existing output files
are protected; add `--force` only to deliberately replace them. Different course
names that produce the same slug are reported as errors even with `--force`;
resolve the names or convert those courses into separate directories and choose
distinct filenames yourself.

After checking the converted files, delete the old course JSON files from the SD
card's `/golf/courses` and copy the files from `courses-v2` there. Keep the local
backup until you have checked course and tee selection on the device. Old v1 files
left on the card will be rejected by v7 firmware.

Run the converter's self-checks with:

```bash
python3 -m unittest discover -s scripts -p test_golf_convert_courses_v2.py
```
