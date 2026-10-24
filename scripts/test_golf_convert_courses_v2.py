"""Run with python3 -m unittest discover -s scripts -p test_golf_convert_courses_v2.py."""

from contextlib import redirect_stdout
import copy
from io import StringIO
import json
from pathlib import Path
import tempfile
import unittest

import golf_convert_courses_v2 as converter


class ConverterTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / "in"
        self.source.mkdir()
        self.dest = Path(self.temp.name) / "out"
        self.course = {"v": 1, "name": "A Course", "holes": 18,
                       "par": [4] * 18, "si": list(range(1, 19)),
                       "tees": "Blue", "yards": list(range(300, 318))}

    def write(self, filename, course):
        (self.source / filename).write_text(json.dumps(course), encoding="utf-8")

    def run_converter(self, force=False):
        captured = StringIO()
        with redirect_stdout(captured):
            result = converter.convert_directory(self.source, self.dest, force)
        return result, captured.getvalue()

    def read(self, filename="a-course.json"):
        return json.loads((self.dest / filename).read_text(encoding="utf-8"))

    def test_grouping_and_filename_order(self):
        white = copy.deepcopy(self.course)
        white["tees"] = "White"
        white["yards"] = [250] * 18
        self.write("z-blue.json", self.course)
        self.write("a-white.json", white)
        self.assertEqual(self.run_converter()[0], 0)
        course = self.read()
        self.assertEqual([n["name"] for n in course["nines"]], ["Out", "In"])
        self.assertEqual([t["name"] for t in course["tees"]], ["White", "Blue"])
        self.assertEqual(course["tees"][1]["yards"], [list(range(300, 309)), list(range(309, 318))])
        self.assertEqual(course["nines"][1]["si"], list(range(10, 19)))
        self.assertIn('[300, 301, 302, 303, 304, 305, 306, 307, 308]',
                      (self.dest / "a-course.json").read_text())

    def test_mismatch_rejects_whole_group(self):
        for field in ("holes", "par", "si"):
            with self.subTest(field=field):
                other = copy.deepcopy(self.course)
                other["tees"] = "White"
                if field == "holes":
                    other["holes"] = 9
                    for key in ("par", "si", "yards"):
                        other[key] = other[key][:9]
                elif field == "par":
                    other["par"][0] = 5
                else:
                    other["si"].reverse()
                self.write("a.json", self.course)
                self.write("b.json", other)
                result, log = self.run_converter()
                self.assertEqual(result, 1)
                self.assertIn(field + " mismatch", log)
                self.assertFalse((self.dest / "a-course.json").exists())

    def test_missing_si_is_a_mismatch(self):
        other = copy.deepcopy(self.course)
        del other["si"]
        self.write("a.json", self.course)
        self.write("b.json", other)
        self.assertIn("si mismatch", self.run_converter()[1])
        self.assertFalse((self.dest / "a-course.json").exists())

    def test_nine_holes(self):
        self.course["holes"] = 9
        for key in ("par", "si", "yards"):
            self.course[key] = self.course[key][:9]
        self.write("nine.json", self.course)
        self.assertEqual(self.run_converter()[0], 0)
        self.assertEqual(len(self.read()["nines"]), 1)
        self.assertEqual(self.read()["tees"][0]["yards"], [self.course["yards"]])

    def test_yardless_tee(self):
        del self.course["yards"]
        self.write("blue.json", self.course)
        self.run_converter()
        self.assertEqual(self.read()["tees"], [{"name": "Blue"}])

    def test_labelless_contributes_no_tee(self):
        for label in (None, ""):
            with self.subTest(label=label):
                if label is None:
                    self.course.pop("tees", None)
                else:
                    self.course["tees"] = label
                self.write("no-label.json", self.course)
                self.run_converter(force=True)
                self.assertEqual(self.read()["tees"], [])

    def test_v2_copied_byte_for_byte(self):
        raw = b'{"v":2,"name":"Existing","nines":null,"tees":null,"extra":true}\n'
        (self.source / "custom-filename.json").write_bytes(raw)
        self.assertEqual(self.run_converter()[0], 0)
        self.assertEqual((self.dest / "custom-filename.json").read_bytes(), raw)

    def test_no_overwrite_without_force(self):
        self.write("a.json", self.course)
        self.dest.mkdir()
        target = self.dest / "a-course.json"
        target.write_text("original")
        self.assertEqual(self.run_converter()[0], 1)
        self.assertEqual(target.read_text(), "original")
        self.assertEqual(self.run_converter(force=True)[0], 0)
        self.assertEqual(self.read()["v"], 2)

    def test_invalid_files_are_skipped(self):
        (self.source / "broken.json").write_text("{")
        self.course["par"] = [0] * 18
        self.write("template.json", self.course)
        result, log = self.run_converter()
        self.assertEqual(result, 0)
        self.assertEqual(list(self.dest.glob("*.json")), [])
        self.assertIn("Skip broken.json", log)
        self.assertIn("Skip template.json", log)

    def test_duplicate_tees_and_slug_collisions_rejected_even_with_force(self):
        self.write("a.json", self.course)
        self.write("b.json", self.course)
        self.assertEqual(self.run_converter()[0], 1)
        other = copy.deepcopy(self.course)
        other["name"] = "A--Course"
        self.write("b.json", other)
        self.assertEqual(self.run_converter(force=True)[0], 1)
        self.assertEqual(list(self.dest.glob("*.json")), [])

    def test_slug_rules(self):
        self.assertEqual(converter.slug("Crème Brûlée"), "cr-me-br-l-e")
        self.assertEqual(converter.slug("İ K Foo"), "foo")
        self.assertEqual(converter.slug(" --Hello__World!-- "), "hello-world")
        self.assertEqual(len(converter.slug("a" * 50)), 40)

    def test_format_preserves_text_that_looks_like_an_array(self):
        course = converter.convert_group([(Path("a.json"), self.course)])
        course["name"] = "A [1,   2] Course"
        self.assertEqual(json.loads(converter.format_course(course)), course)

    def test_numeric_and_text_validation(self):
        for field, value in (("v", True), ("holes", 18.0), ("name", "a\nb"),
                             ("tees", "Bad,tee"), ("par", [True] * 18),
                             ("yards", [1000] * 18), ("si", [1] * 18)):
            with self.subTest(field=field):
                course = dict(self.course, **{field: value})
                self.assertIsNotNone(converter.validate_v1(course))


if __name__ == "__main__":
    unittest.main()
