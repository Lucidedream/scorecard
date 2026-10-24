#!/usr/bin/env python3
"""Offline §41 acceptance, exclusively invented fixtures and generated images."""
import argparse
import copy
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import urllib.error
import unittest
from unittest.mock import patch

from baigolf_common import external_path, read_json, write_json
import baigolf_export as ex
from baigolf_scrape import Fetcher, SitemapParser, scrape

FIXTURES = Path(__file__).parent / 'fixtures/baigolf'


class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='synthetic-baigolf-')
        self.root = Path(self.temp.name)
        self.cache, self.out = self.root / 'cache', self.root / 'out'
        self.fixture = read_json(FIXTURES / 'course.json')
        self.detail = copy.deepcopy(self.fixture['detail'])
        self.cards = copy.deepcopy(self.fixture['scorecards'])
        self.entry = dict(id=7001, region_zh='江苏', name_zh='Invented')
        self.report = []

    def tearDown(self):
        self.temp.cleanup()

    def build(self):
        return ex.build_course(self.entry, self.detail, self.cards, self.report)[0]

    def cache_course(self, cid=7001, region='江苏', detail=None):
        entries = read_json(self.cache / 'sitemap.json') if (self.cache / 'sitemap.json').exists() else []
        entries.append(dict(id=cid, region_zh=region, name_zh='Invented'))
        write_json(self.cache / 'sitemap.json', entries)
        write_json(self.cache / 'detail' / f'{cid}.json', dict(succeed=1, data=detail or self.detail))
        write_json(self.cache / 'scorecards' / f'{cid}.json', dict(succeed=1, data=self.cards))

    def row(self, nine, name):
        return next(r for r in self.cards['list'][nine]['scorecard'] if r['name'] == name)

    def images(self, titles):
        return [dict(id=8000 + i, images_title=t) for i, t in enumerate(titles)]

    def test_sitemap_china_only(self):
        parser = SitemapParser()
        parser.feed((FIXTURES / 'sitemap.html').read_text())
        self.assertEqual([(c['id'], c['region_zh']) for c in parser.courses], [(7001, '江苏'), (7002, '浙江')])

    def test_nine_names_and_total(self):
        course = self.build()
        self.assertEqual([n['name'] for n in course['nines']], ['Out', 'In'])
        self.assertEqual(course['nines'][0]['par'], [4] * 9)
        for table, name in zip(self.cards['list'], ['ＯＣＥＡＮ COURSE', 'Lake Course']):
            table['name'] = name
        self.assertEqual([n['name'] for n in self.build()['nines']], ['Ocean', 'Lake'])

    def test_lettered_table_holes(self):
        self.row(0, 'HOLE')['values'] = [f'A{i}' for i in range(1, 10)] + ['TOTAL']
        self.assertEqual(len(self.build()['nines']), 2)

    def test_eighteen_hole_split(self):
        self.cards['list'] = self.cards['list'][:1]
        for row in self.cards['list'][0]['scorecard']:
            row['values'] = (list(range(1, 19)) if row['name'] == 'HOLE' else row['values'][:9] * 2) + ['TOTAL']
        self.assertEqual([n['name'] for n in self.build()['nines']], ['Out', 'In'])
        self.assertEqual(len(self.build()['tees'][0]['yards']), 2)

    def test_bad_nine_names_fallback(self):
        for names in [('Repeated', 'Repeated'), ('Way Too Long Nine', 'In'), ('Bad,Name', 'In'), ('', 'In')]:
            for table, name in zip(self.cards['list'], names):
                table['name'] = name
            self.assertEqual([n['name'] for n in self.build()['nines']], ['Nine 1', 'Nine 2'])

    def test_invalid_nines_and_par(self):
        for count in (0, 5):
            self.cards['list'] = copy.deepcopy(self.fixture['scorecards']['list'][:1]) * count
            with self.assertRaises(ValueError):
                self.build()
        self.cards = copy.deepcopy(self.fixture['scorecards'])
        for value in (2, 7, '', '3.5'):
            self.row(0, 'Par')['values'][0] = value
            with self.assertRaises(ValueError):
                self.build()

    def test_si_drop_on_every_nine(self):
        self.assertTrue(all('si' in n for n in self.build()['nines']))
        for values in ([1] * 9, [''] * 9, list(range(11, 20))):
            self.row(1, 'HDCP')['values'] = values
            self.assertTrue(all('si' not in n for n in self.build()['nines']))
        self.cards['list'] = self.cards['list'][:1]
        self.row(0, 'HDCP')['values'] = list(range(10, 19))
        self.assertNotIn('si', self.build()['nines'][0])

    def test_tee_mapping_yardless_and_order(self):
        self.row(1, '白')['values'][0] = 0
        tees = self.build()['tees']
        self.assertEqual([t['name'] for t in tees], ['Blue', 'Red', 'White'])
        self.assertNotIn('yards', tees[-1])
        self.cards['list'][0]['scorecard'].append(dict(name='Yellow', values=[222] * 9))
        self.assertNotIn('yards', next(t for t in self.build()['tees'] if t['name'] == 'Yellow'))

    def test_tee_validation_max_and_omission(self):
        for table in self.cards['list']:
            table['scorecard'] = table['scorecard'][:3] + [dict(name=n, values=[300] * 9) for n in ['黑', '金', '蓝', '白', '红', '黄', '绿', '银', 'Bad,Name', 'Too Long Tee Name']]
        self.assertEqual([t['name'] for t in self.build()['tees']], ['Black', 'Gold', 'Blue', 'White', 'Green', 'Yellow'])
        self.assertTrue(any('dropped tee' in r for r in self.report))
        for table in self.cards['list']:
            table['scorecard'] = table['scorecard'][:3]
        self.assertNotIn('tees', self.build())

    def test_name_normalization_and_fallback(self):
        self.detail['course_name_en'] = 'Ｊｉａｎｇｓｕ  Fictional\tHeron Golf &Country Club'
        self.assertEqual(self.build()['name'], 'Fictional Heron Golf & Country Club')
        self.detail['golf_course_name_en'] = self.detail.pop('course_name_en')
        self.assertTrue(self.build()['name'].startswith('Fictional'))
        self.detail.pop('golf_course_name_en')
        with self.assertRaises(ValueError):
            self.build()

    def test_name_shorten_and_invalid(self):
        self.detail['course_name_en'] = 'Imaginary Heron International Golf & Country Club'
        self.assertEqual(self.build()['name'], 'Imaginary Heron International G&CC')
        self.detail['course_name_en'] = 'Imaginary ' * 12 + 'Club'
        name = self.build()['name']
        self.assertLessEqual(len(name.encode()), 39)
        self.assertFalse(name.endswith((' ', '-', '&')))
        for name in ('Bad|Name', 'Bad,Name'):
            self.detail['course_name_en'] = name
            with self.assertRaises(ValueError):
                self.build()

    def test_name_removes_parentheses_before_abbreviations(self):
        cases = [
            ('Imaginary Golf Course (formerly Heron Golf Course)', 'Imaginary Golf Course'),
            ('Imaginary Heron Lakeside Course (new course)', 'Imaginary Heron Lakeside Course'),
            ('Imaginary International Golf Club (formerly Heron)', 'Imaginary International Golf Club'),
            ('Imaginary Golf Course (old) (new extension)', 'Imaginary Golf Course (old)'),
            ('Imaginary Golf Course (old (renamed) course)', 'Imaginary Golf Course'),
        ]
        for source, expected in cases:
            with self.subTest(source=source):
                self.detail['course_name_en'] = source
                self.assertEqual(self.build()['name'], expected)

    def test_short_names_keep_parentheses_after_normalization(self):
        for source, expected in [('Heron Course (new course)', 'Heron Course (new course)'),
                                 ('Heron Course （Old）', 'Heron Course (Old)'),
                                 ('Heron 3(The Vintage)', 'Heron 3(The Vintage)')]:
            with self.subTest(source=source):
                self.detail['course_name_en'] = source
                self.assertEqual(self.build()['name'], expected)

    def test_cut_and_duplicate_suffix_never_leave_open_parentheses(self):
        self.assertEqual(ex.shorten('Imaginary Heron Course 3(The Vintage)', 30), 'Imaginary Heron Course 3')
        self.assertEqual(ex.shorten('Imaginary Course (old name) Extra Words', 32), 'Imaginary Course (old name)')
        self.assertEqual(ex.shorten('Heron (old (original) name) Extra Words', 20), 'Heron')
        self.assertEqual(ex.remove_parenthesized('(Only the invented course name)', 12), '(Only the invented course name)')
        name = 'Heron Golf Course (' + 'A' * 19 + ')'
        self.assertEqual(len(name), 39)
        self.detail['course_name_en'] = name
        self.cache_course(7001)
        self.cache_course(7002)
        courses = ex.export(self.cache, self.out)
        self.assertEqual([c['name'] for c in courses], [name, 'Heron Golf Course 2'])

    def test_numbered_parenthetical_courses_stay_distinct_after_dedupe(self):
        for cid, number, nickname in [(7001, 3, 'The Vintage'), (7002, 9, 'The Preserve'), (7003, 3, 'The Vintage')]:
            detail = copy.deepcopy(self.detail)
            detail['course_name_en'] = f'Imaginary Hills Valley Golf Course {number}({nickname})'
            self.cache_course(cid, detail=detail)
        courses = ex.export(self.cache, self.out)
        names = {c['baigolf']['id']: c['name'] for c in courses}
        self.assertEqual(names, {7001: 'Imaginary Hills Valley Golf Course 3',
                                 7002: 'Imaginary Hills Valley Golf Course 9',
                                 7003: 'Imaginary Hills Valley Golf Course 3 2'})
        self.assertEqual(len(set(name.casefold() for name in names.values())), 3)
        for name in names.values():
            self.assertLessEqual(len(name.encode()), 39)
            self.assertEqual(name.count('('), name.count(')'))

    def test_metadata(self):
        meta = self.build()['baigolf']
        self.assertEqual((meta['rating'], meta['reviews'], meta['scores']), (43, 17, [38] * 6))
        self.assertEqual((meta['holes'], meta['par'], meta['yards'], meta['opened']), (18, 72, 6543, 2007))
        self.assertEqual(meta['designer'], 'Imaginary Architect')
        self.detail['total_eval']['peop_total'] = 0
        self.detail['total_eval'].pop('course_diff')
        self.detail.update(designer='建築家', build_time='unknown', yardage='', distance='6123', holes='0')
        meta = self.build()['baigolf']
        for key in ('rating', 'reviews', 'scores', 'opened', 'designer', 'holes'):
            self.assertNotIn(key, meta)
        self.assertEqual(meta['yards'], 6123)

    def test_image_numeric_lettered_fullwidth_overview(self):
        items = self.images([str(i) for i in range(1, 19)] + ['overview-hash'])
        self.assertEqual(sorted(ex.image_holes(items, 2)), list(range(1, 19)))
        titles = [str(i) for i in range(1, 10)] + [f'Ｂ{i}' for i in range(1, 10)] + [f'Ｃ{i}' for i in range(1, 10)]
        self.assertEqual(len(ex.image_holes(self.images(titles), 3)), 27)
        self.assertEqual(ex.image_holes(self.images(['1'] * 18), 2), {})
        self.assertEqual(len(ex.image_holes(self.images([str(i) for i in range(1, 18)]), 2)), 17)
        self.assertEqual(sorted(ex.image_holes(self.images(['A1', 'B1']), 2)), [1, 10])

    def test_image_title_suffix_and_prefix_normalization(self):
        for pattern in ('{} 洞', '第{}洞', 'Hole {}', 'H {}', '第{}号洞', '{}号'):
            self.assertEqual(len(ex.image_holes(self.images([pattern.format(i) for i in range(1, 19)]), 2)), 18)

    def test_image_last_digit_run_and_nonoverlapping_group_ranges(self):
        titles = [f'A场1-9号-{i}' for i in range(1, 10)] + [f'B场1-9号-{i}' for i in range(10, 19)]
        items = self.images(titles)
        mapping = ex.image_holes(items, 2)
        self.assertEqual(sorted(mapping), list(range(1, 19)))
        for hole, item in mapping.items():
            self.assertEqual(item['id'], items[hole-1]['id'])

    def test_image_letters_hyphens_and_letter_order(self):
        for pattern in ('{letter}{hole}', '{letter}-{hole}', '{letter}场{hole}'):
            with self.subTest(pattern=pattern):
                titles = [pattern.format(letter=letter, hole=hole) for letter in ('Ｃ', 'b', 'A') for hole in range(1, 10)]
                items = self.images(titles)
                report = []
                mapping = ex.image_holes(items, 3, report)
                self.assertEqual(sorted(mapping), list(range(1, 28)))
                self.assertEqual(mapping[1]['id'], items[18]['id'])
                self.assertEqual(mapping[19]['id'], items[0]['id'])
                self.assertFalse(any('order by appearance' in line for line in report))

    def test_image_eighteen_hole_groups_match_raw_nine_names(self):
        items = self.images([f'北{i}' for i in range(1, 19)] + [f'南{i}' for i in range(1, 19)])
        names = ['South Course Out', 'South Course In', 'North Course Out', 'North Course In']
        mapping = ex.image_holes(items, 4, nine_names=names)
        self.assertEqual(sorted(mapping), list(range(1, 37)))
        self.assertEqual(mapping[1]['id'], items[18]['id'])
        self.assertEqual(mapping[19]['id'], items[0]['id'])
        interleaved = ['South Out', 'North Out', 'South In', 'North In']
        mapping = ex.image_holes(items, 4, nine_names=interleaved)
        self.assertEqual(mapping[10]['id'], items[0]['id'])
        self.assertEqual(mapping[28]['id'], items[9]['id'])
        self.assertEqual(ex.image_holes(items, 3), {})

    def test_image_partial_sets_and_descending_numbers(self):
        report = []
        items = self.images([f'{i:02} 洞' for i in range(18, 0, -1)])
        mapping = ex.image_holes(items, 3, report)
        self.assertEqual(sorted(mapping), list(range(1, 19)))
        self.assertEqual(mapping[18]['id'], items[0]['id'])
        self.assertEqual(mapping[1]['id'], items[-1]['id'])
        self.assertFalse(any('order by appearance' in line for line in report))
        partial = self.images(['B-2', 'A-1', 'B-9'])
        self.assertEqual(sorted(ex.image_holes(partial, 2)), [1, 2, 9])
        partial18 = self.images(['Second-18', 'First-10'])
        self.assertEqual(sorted(ex.image_holes(partial18, 4)), [10, 18])

    def test_image_camera_filenames_zero_and_out_of_range_ignored(self):
        titles = ['微信图片_20260304112233', 'Camera_123456-01', '微信图片_20260304112233-2', '0', 'Hole 19', 'overview']
        self.assertEqual(ex.image_holes(self.images(titles), 2), {})
        self.assertEqual(sorted(ex.image_holes(self.images(titles + ['第1号洞']), 2)), [1])

    def test_image_mixed_ranges_are_ambiguous(self):
        titles = [f'c{i}' for i in range(1, 10)] + [str(i) for i in range(1, 19)]
        report = []
        self.assertEqual(ex.image_holes(self.images(titles), 3, report), {})
        self.assertTrue(any('ambiguous' in line for line in report))
        self.assertEqual(ex.image_holes(self.images(['A1', 'B1', 'C1']), 2), {})

    def test_image_duplicate_holes_dropped_without_losing_other_maps(self):
        report = []
        mapping = ex.image_holes(self.images(['1', '2', '2', '3']), 2, report)
        self.assertEqual(sorted(mapping), [1, 3])
        self.assertTrue(any('duplicate holes: [2]' in line for line in report))
        self.assertEqual(sorted(ex.image_holes(self.images(['B1', 'A1', 'A1', 'B2']), 2)), [10, 11])

    def test_image_unknown_groups_have_no_appearance_fallback(self):
        for titles in (['A场1', 'Another1'], ['A场1', 'AB场1'], ['虚构A1', '虚构B1']):
            report = []
            self.assertEqual(ex.image_holes(self.images(titles), 2, report), {})
            self.assertFalse(any('order by appearance' in line for line in report))
            self.assertTrue(any('no maps' in line for line in report))

    def test_image_compass_groups_follow_scorecard_order(self):
        cases = [
            (['北', '东', '西', '中'], ['North Course', 'West Course', 'East Course', 'Center Course'], [0, 2, 1, 3]),
            (['m', 'n', 's'], ['North Course', 'Middle Course', 'South Course'], [1, 0, 2]),
            (['east', 'mid', 'west'], ['East Course', 'West Course', 'Center Course'], [0, 2, 1]),
            (['E', 'W', 'N'], ['North Course', 'East Course', 'West Course'], [2, 0, 1]),
        ]
        for keys, names, source_groups in cases:
            with self.subTest(keys=keys):
                items = self.images([f'{key}-{hole}' for key in keys for hole in range(1, 10)])
                mapping = ex.image_holes(items, len(names), nine_names=names)
                self.assertEqual(sorted(mapping), list(range(1, 9 * len(names) + 1)))
                for nine, source in enumerate(source_groups):
                    self.assertEqual(mapping[9 * nine + 1]['id'], items[9 * source]['id'])

    def test_image_empty_key_means_a_only_among_letters(self):
        items = self.images([str(i) for i in range(1, 10)] + [f'B{i}' for i in range(1, 10)])
        mapping = ex.image_holes(items, 2, nine_names=['A Course', 'B Course'])
        self.assertEqual(len(mapping), 18)
        self.assertEqual(ex.image_holes(items, 2, nine_names=['B Course', 'C Course']), {})
        named = self.images([f'{letter}{hole}' for letter in ['D', 'C'] for hole in range(1, 10)])
        mapping = ex.image_holes(named, 2, nine_names=['C Course', 'D Course'])
        self.assertEqual(len(mapping), 18)
        self.assertEqual(mapping[1]['id'], named[9]['id'])
        self.assertEqual(mapping[10]['id'], named[0]['id'])
        numeric_north = self.images(['1', 'N1'])
        self.assertEqual(ex.image_holes(numeric_north, 2, nine_names=['North', 'South']), {})

    def test_image_unmatched_or_duplicate_semantic_nines_reject_all_maps(self):
        cases = [(['虚构东赛', '虚构西赛'], ['A Course', 'B Course', 'C Course', 'D Course'], 18),
                 (['幻湖', '幻林', '幻谷'], ['A', 'B', 'C'], 9),
                 (['B', 'N'], ['North Course Out', 'North Course In', 'South Course Out', 'South Course In'], 18),
                 (['north', 'n'], ['North Course', 'South Course'], 9)]
        for keys, names, holes in cases:
            with self.subTest(keys=keys):
                items = self.images([f'{key}{hole}' for key in keys for hole in range(1, holes + 1)])
                self.assertEqual(ex.image_holes(items, len(names), nine_names=names), {})

    def test_image_letter_fallback_requires_token_free_raw_names_and_fixed_positions(self):
        items = self.images([f'{key}{hole}' for key in ['C', 'B'] for hole in range(1, 10)])
        names = ['Oak', 'River', 'Meadow']
        report = []
        mapping = ex.image_holes(items, 3, report, nine_names=names)
        self.assertTrue(any('letters-only fallback' in line for line in report))
        self.assertEqual(sorted(mapping), list(range(10, 28)))
        self.assertEqual(mapping[10]['id'], items[9]['id'])
        self.assertEqual(ex.image_holes(items, 3, nine_names=['North', 'River', 'Meadow']), {})
        self.assertEqual(ex.image_holes(items, 2, nine_names=['Oak', 'River']), {})
        repeated = self.images(['A1', 'a1', 'B1'])
        self.assertEqual(ex.image_holes(repeated, 3, nine_names=names), {})

    def test_image_out_in_tokens_and_incidental_letters_do_not_guess(self):
        items = self.images(['IN1', 'OUT1'])
        mapping = ex.image_holes(items, 2, nine_names=['Out', 'In'])
        self.assertEqual(mapping[10]['id'], items[0]['id'])
        self.assertEqual(mapping[1]['id'], items[1]['id'])
        for value in ['Spring', 'Links', 'Origin', '中山', 'WillowA', '幻湖']:
            self.assertIsNone(ex.image_token(value, group_key=True), value)
        for value, token in [('NorthCourseOut', 'N'), ('West Course In', 'W'), ('A Course Out', 'A'),
                             ('A-course', 'A'), ('前', 'OUT'), ('后', 'IN'), ('Middle', 'M')]:
            self.assertEqual(ex.image_token(value), token)
        self.assertEqual(ex.image_token('North South'), '?')

    def test_raw_scorecard_table_names_survive_splitting_and_ui_name_fallback(self):
        for table in self.cards['list']:
            for row in table['scorecard']:
                row['values'] = (list(range(1, 19)) if row['name'] == 'HOLE' else row['values'][:9] * 2) + ['TOTAL']
        self.cards['list'][0]['name'] = 'South Course'
        self.cards['list'][1]['name'] = 'North Course'
        self.assertEqual(ex.raw_nine_names(self.cards), ['South Course', 'South Course', 'North Course', 'North Course'])
        course = self.build()
        self.assertEqual([nine['name'] for nine in course['nines']], ['Nine 1', 'Nine 2', 'Nine 3', 'Nine 4'])
        items = self.images([f'N{i}' for i in range(1, 19)] + [f'S{i}' for i in range(1, 19)])
        mapping = ex.image_holes(items, 4, nine_names=ex.raw_nine_names(self.cards))
        self.assertEqual(mapping[1]['id'], items[18]['id'])
        self.assertEqual(mapping[19]['id'], items[0]['id'])

    def mapped_tones(self, colors):
        image = ex.pillow().new('RGB', (len(colors), 1))
        image.putdata(colors)
        mapped = ex.tone_map(image)
        self.assertEqual(mapped.mode, 'L')
        return list(mapped.tobytes())

    def test_tone_map_path_or_white(self):
        self.assertEqual(self.mapped_tones([(255, 255, 255), (224, 224, 224), (220, 240, 240)]),
                         [255, 255, 255])

    def test_tone_map_water(self):
        self.assertEqual(self.mapped_tones([(0, 128, 255), (0, 128, 128)]), [20, 20])

    def test_tone_map_sand(self):
        self.assertEqual(self.mapped_tones([(255, 192, 0), (204, 153, 0)]), [245, 245])

    def test_tone_map_grass_or_trees(self):
        self.assertEqual(self.mapped_tones([(0, 255, 0), (0, 128, 0), (0, 64, 0)]), [250, 123, 81])

    def test_tone_map_other(self):
        self.assertEqual(self.mapped_tones([(255, 0, 0), (128, 0, 0), (0, 0, 0)]), [200, 100, 0])

    def test_tone_map_class_boundaries_and_precedence(self):
        self.assertEqual(ex.hsv_tone(0.6, 0.17, 0.8), 255)
        self.assertEqual(ex.hsv_tone(0.6, 0.18, 0.8), 160)
        self.assertEqual(ex.hsv_tone(0, 0, 0.75), 150)
        self.assertEqual(ex.hsv_tone(0.6, 0.30, 0.5), 100)
        self.assertEqual(ex.hsv_tone(0.72, 1, 1), 200)
        self.assertEqual(ex.hsv_tone(0.08, 1, 1), 200)
        self.assertEqual(ex.hsv_tone(0.12, 1, 0.60), 120)
        self.assertEqual(ex.hsv_tone(0.17, 1, 1), 250)
        self.assertEqual(ex.hsv_tone(0.45, 1, 1), 250)

    def test_bitmap_headers_pixels_and_scaling(self):
        Image = ex.pillow()
        image = Image.new('L', (3, 5), 255)
        image.putpixel((0, 4), 0)
        image.save(self.root / 'tiny.png')
        for bpp in (2, 8):
            data = ex.bitmap(self.root / 'tiny.png', bpp)
            start = struct.unpack_from('<I', data, 10)[0]
            width, height = struct.unpack_from('<ii', data, 18)
            self.assertEqual((width, height), (384, 640))
            self.assertEqual(struct.unpack_from('<H', data, 28)[0], bpp)
            self.assertEqual(struct.unpack_from('<I', data, 30)[0], 0)
            self.assertEqual(data[:2], b'BM')
            self.assertEqual(struct.unpack_from('<I', data, 2)[0], len(data))
            self.assertEqual(data[start], 0)
            self.assertEqual(data[start - 4:start], bytes((255, 255, 255, 0)))
            self.assertEqual(len(data) - start, ((width * bpp + 31) // 32) * 4 * height)

    def test_index_offsets_sort_regions_dedupe_and_determinism(self):
        self.cache_course(cid=7002)
        self.cache_course(cid=7001)
        other = copy.deepcopy(self.detail)
        other['course_name_en'] = 'Aster Imaginary Club'
        other['total_eval']['peop_total'] = 0
        self.cache_course(cid=7003, region='浙江', detail=other)
        courses = ex.export(self.cache, self.out)
        self.assertEqual([c['baigolf']['id'] for c in courses], [7001, 7002, 7003])
        self.assertEqual(courses[1]['name'], courses[0]['name'] + ' 2')
        record = (self.out / 'golf/catalog/index.dat').read_bytes()
        self.assertEqual(len(record), 288)
        self.assertEqual(record[:29], b'00007001|00|2|072|43|00017|N|')
        self.assertEqual(record[29:69], courses[0]['name'].encode().ljust(40))
        self.assertEqual(record[69:96], b'|' + b' ' * 25 + b'\n')
        self.assertEqual(record[192 + 18:192 + 20], b'--')
        lines = (self.out / 'golf/catalog/regions.txt').read_text().splitlines()
        self.assertRegex(lines[0], r'^# golf-catalog 1 \d{4}-\d{2}-\d{2} 2 3$')
        self.assertEqual(lines[1:], ['00|Jiangsu|2|0', '01|Zhejiang|1|2'])
        before = {p.relative_to(self.out): p.read_bytes() for p in self.out.rglob('*') if p.is_file()}
        ex.export(self.cache, self.out, force=True)
        self.assertEqual(before, {p.relative_to(self.out): p.read_bytes() for p in self.out.rglob('*') if p.is_file()})
        with self.assertRaisesRegex(ValueError, '--force'):
            ex.export(self.cache, self.out)

    def test_export_maps_report_owner_preserved_and_stale_cleanup(self):
        self.detail['ball_path_img_list'] = self.images([str(i) for i in range(1, 19)])
        self.cache_course()
        Image = ex.pillow()
        for item in self.detail['ball_path_img_list']:
            target = self.cache / 'images/7001' / f"{item['id']}.png"
            target.parent.mkdir(parents=True, exist_ok=True)
            Image.new('L', (2, 3), 170).save(target)
        write_json(self.out / 'golf/courses/mine.json', {'name': 'Imaginary Heron Club'})
        sentinel = self.out / 'golf/rounds/preserve.txt'
        sentinel.parent.mkdir(parents=True)
        sentinel.write_text('preserve')
        ex.export(self.cache, self.out)
        self.assertEqual(len(list((self.out / 'golf/maps/bg-7001').glob('*.bmp'))), 18)
        self.assertEqual((self.out / 'golf/catalog/index.dat').read_bytes()[27:28], b'Y')
        report = (self.cache / 'export-report.txt').read_text()
        self.assertIn('suggested owner duplicate', report)
        self.assertIn('7001: maps resolved 18/18; exported 18/18', report)
        self.assertEqual(sentinel.read_text(), 'preserve')
        before = {p.name: p.read_bytes() for p in (self.out / 'golf/maps/bg-7001').glob('*.bmp')}
        ex.export(self.cache, self.out, force=True)
        self.assertEqual(before, {p.name: p.read_bytes() for p in (self.out / 'golf/maps/bg-7001').glob('*.bmp')})
        self.detail['ball_path_img_list'] = []
        write_json(self.cache / 'detail/7001.json', dict(succeed=1, data=self.detail))
        ex.export(self.cache, self.out, force=True)
        self.assertFalse((self.out / 'golf/maps/bg-7001').exists())
        self.assertIn('7001: maps resolved 0/18; exported 0/18', (self.cache / 'export-report.txt').read_text())

    def test_unknown_region_loud_and_skipped_report(self):
        self.cache_course(region='Unknown Region')
        with self.assertRaisesRegex(ValueError, 'unknown regions'):
            ex.export(self.cache, self.out)
        write_json(self.cache / 'sitemap.json', [self.entry])
        write_json(self.cache / 'detail/7001.json', dict(succeed=0))
        self.assertEqual(ex.export(self.cache, self.out), [])
        self.assertIn('skipped', (self.cache / 'export-report.txt').read_text())

    def test_git_paths_existing_new_and_symlink(self):
        git = self.root / 'git'
        git.mkdir()
        subprocess.run(['git', 'init', '-q', str(git)], check=True)
        (self.root / 'link').symlink_to(git, target_is_directory=True)
        for path in (git, git / 'new/cache', self.root / 'link/out'):
            with self.assertRaisesRegex(ValueError, 'outside git'):
                external_path(path)
        self.cache_course()
        with self.assertRaisesRegex(ValueError, 'outside git'):
            ex.export(self.cache, git)
        with self.assertRaisesRegex(ValueError, 'outside git'):
            ex.export(git, self.out)

    def test_missing_pillow_message(self):
        with patch.dict('sys.modules', {'PIL': None}):
            with self.assertRaisesRegex(ValueError, '~/.platformio/penv/bin/python'):
                ex.pillow()

    def test_fetch_cache_atomic_and_politeness(self):
        response = unittest.mock.MagicMock()
        response.__enter__.return_value.read.return_value = b'{"succeed":1,"data":{}}'
        fetcher = Fetcher(2)
        with patch('urllib.request.urlopen', return_value=response) as urlopen, patch('time.sleep') as sleep:
            first = fetcher.fetch('https://synthetic.invalid/api', self.root / 'one.json', api=True)
            self.assertEqual(first['succeed'], 1)
            fetcher.fetch('https://synthetic.invalid/api', self.root / 'one.json', api=True)
            fetcher.fetch('https://synthetic.invalid/two', self.root / 'two.json', api=True)
            self.assertEqual(urlopen.call_count, 2)
            self.assertEqual(urlopen.call_args[0][0].headers['User-agent'], 'scorecard-catalog/1 (personal use)')
            self.assertGreater(sleep.call_args[0][0], 1)
        self.assertFalse(list(self.root.glob('*.tmp')))

    def test_http_block_retry_backoff(self):
        for code in (403, 429):
            error = urllib.error.HTTPError('https://synthetic.invalid', code, 'blocked', {}, None)
            with patch('urllib.request.urlopen', side_effect=error) as urlopen, patch('time.sleep') as sleep:
                with self.assertRaises(urllib.error.HTTPError):
                    Fetcher(2).fetch('https://synthetic.invalid/api', self.root / 'blocked.json')
                self.assertEqual(urlopen.call_count, 3)
                self.assertEqual(sum(c.args == (60,) for c in sleep.call_args_list), 2)
            self.assertFalse((self.root / 'blocked.json').exists())

    def test_duplicate_image_url_fetched_once(self):
        response = unittest.mock.MagicMock()
        response.__enter__.return_value.read.return_value = b'synthetic image bytes'
        fetcher = Fetcher(2)
        with patch('urllib.request.urlopen', return_value=response) as urlopen:
            fetcher.fetch('https://synthetic.invalid/same.png', self.root / 'one.png')
            fetcher.fetch('https://synthetic.invalid/same.png', self.root / 'two.png')
            self.assertEqual(urlopen.call_count, 1)
        self.assertEqual((self.root / 'one.png').read_bytes(), (self.root / 'two.png').read_bytes())

    def test_scrape_records_api_failure_offline(self):
        self.cache.mkdir()
        (self.cache / 'sitemap.html').write_text((FIXTURES / 'sitemap.html').read_text())
        write_json(self.cache / 'detail/7001.json', {'succeed': 0})
        with patch('urllib.request.urlopen', side_effect=AssertionError('unexpected network')):
            scrape(argparse.Namespace(cache=self.cache, ids='7001', no_images=True,
                                      refresh_details=False, delay=2, limit=None))
        self.assertIn('7001', read_json(self.cache / 'errors.json'))

    def test_non_json_retry_backoff(self):
        response = unittest.mock.MagicMock()
        response.__enter__.return_value.read.return_value = b'<html>blocked</html>'
        with patch('urllib.request.urlopen', return_value=response) as urlopen, patch('time.sleep') as sleep:
            with self.assertRaisesRegex(ValueError, 'resum'):
                Fetcher(2).fetch('https://synthetic.invalid/api', self.root / 'no.json', api=True)
            self.assertEqual(urlopen.call_count, 3)
            self.assertEqual(sum(c.args == (60,) for c in sleep.call_args_list), 2)
        self.assertFalse((self.root / 'no.json').exists())


if __name__ == '__main__':
    unittest.main()
