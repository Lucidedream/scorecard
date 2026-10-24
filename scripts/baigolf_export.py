#!/usr/bin/env python3
"""Export a personal baigolf cache to an offline SD catalog (§41.3)."""
import argparse
import colorsys
from decimal import Decimal, InvalidOperation, ROUND_HALF_UP
import datetime
import re
import shutil
import struct
import unicodedata

from baigolf_common import DEFAULT_CACHE, atomic_write, external_path, payload, read_json, write_json

REGIONS = dict(zip(
    '上海 中国台湾 云南 内蒙古 北京 吉林 四川 天津 宁夏 安徽 山东 山西 广东 广西 新疆 江苏 江西 河北 河南 浙江 海南 湖北 湖南 澳门 福建 贵州 辽宁 重庆 陕西 青海 香港'.split(),
    ['Shanghai', 'Taiwan', 'Yunnan', 'Inner Mongolia', 'Beijing', 'Jilin', 'Sichuan', 'Tianjin',
     'Ningxia', 'Anhui', 'Shandong', 'Shanxi', 'Guangdong', 'Guangxi', 'Xinjiang', 'Jiangsu',
     'Jiangxi', 'Hebei', 'Henan', 'Zhejiang', 'Hainan', 'Hubei', 'Hunan', 'Macau', 'Fujian',
     'Guizhou', 'Liaoning', 'Chongqing', 'Shaanxi', 'Qinghai', 'Hong Kong']))
COLORS = dict(zip('黑 金 蓝 白 红 黄 绿 银'.split(), 'Black Gold Blue White Red Yellow Green Silver'.split()))
PRIORITY = 'Black Gold Blue White Green Yellow Red Orange'.split()
SCORES = 'green_fairway course_diff caddie_service price_exp club_facilities catering_qlty'.split()

WHITE_SATURATION_MAX = 0.18
WHITE_VALUE_MIN = 0.75
WATER_HUE_MIN = 0.45
WATER_HUE_MAX = 0.72
WATER_SATURATION_MIN = 0.30
SAND_HUE_MIN = 0.08
SAND_HUE_MAX = 0.17
SAND_VALUE_MIN = 0.60
GRASS_HUE_MIN = 0.17
GRASS_HUE_MAX = 0.45
WHITE_TONE = 255
WATER_TONE = 20
SAND_TONE = 245
GRASS_TONE_BASE = 60
GRASS_TONE_RANGE = 190
GRASS_VALUE_EXPONENT = 1.6
OTHER_TONE_RANGE = 200


def pillow():
    try:
        from PIL import Image
        return Image
    except ImportError as error:
        raise ValueError('Pillow is required; run ~/.platformio/penv/bin/python scripts/baigolf_export.py') from error


def nfkc(value):
    return unicodedata.normalize('NFKC', str(value))


def valid_text(value, size, forbidden=',\r\n'):
    return bool(value) and len(value.encode('utf-8')) <= size and not any(c in value for c in forbidden + '\0')


def integers(values, minimum, maximum):
    try:
        result = [int(str(v)) for v in values]
    except (ValueError, TypeError):
        return None
    return result if len(result) == 9 and all(minimum <= v <= maximum for v in result) else None


def shorten(value, limit):
    if len(value.encode()) <= limit:
        return value
    prefix = value.encode()[:limit].decode('utf-8', errors='ignore')
    if ' ' in prefix:
        prefix = prefix.rsplit(' ', 1)[0]
    openings = []
    for index, char in enumerate(prefix):
        if char == '(':
            openings.append(index)
        elif char == ')' and openings:
            openings.pop()
    if openings:
        prefix = prefix[:openings[0]]
    return prefix.rstrip(' &-(')


def remove_parenthesized(value, limit):
    spans, opening, depth = [], 0, 0
    for index, char in enumerate(value):
        if char == '(':
            if depth == 0:
                opening = index
            depth += 1
        elif char == ')' and depth:
            depth -= 1
            if depth == 0:
                spans.append((opening, index + 1))
    outside = value
    for start, end in reversed(spans):
        outside = outside[:start] + outside[end:]
    if not outside.strip():
        return value
    for start, end in reversed(spans):
        if len(value.encode()) <= limit:
            break
        value = ' '.join((value[:start] + value[end:]).split())
    return value


def course_name(detail, region):
    source = detail.get('course_name_en') or detail.get('golf_course_name_en')
    if not source:
        raise ValueError('missing English name')
    name = ' '.join(nfkc(source).replace('&', ' & ').split())
    if name.casefold().startswith(region.casefold() + ' '):
        rest = name[len(region):].strip()
        if len(rest.split()) >= 2:
            name = rest
    name = remove_parenthesized(name, 39)
    for old, new in [('Golf & Country Club', 'G&CC'), ('Golf and Country Club', 'G&CC'),
                     ('International', 'Intl'), ('Country Club', 'CC'), ('Golf Club', 'GC')]:
        if len(name.encode()) > 39:
            name = name.replace(old, new)
    name = shorten(name, 39)
    if not valid_text(name, 39, '|,\r\n') or not name.isascii():
        raise ValueError('invalid English name')
    return name, source


def build_course(entry, detail, scorecards, report):
    cid = entry['id']
    region = REGIONS[entry['region_zh']]
    name, source = course_name(detail, region)
    nines, rows_by_nine = [], []
    for table in scorecards['list']:
        rows = {nfkc(r['name']): r['values'] for r in table['scorecard']}
        holes = rows.get('HOLE', [])
        count = len(holes) - (1 if holes and str(holes[-1]).upper() == 'TOTAL' else 0)
        if count not in (9, 18):
            raise ValueError('table must have 9 or 18 holes')
        for offset in range(0, count, 9):
            nine_name = ('Out' if offset == 0 else 'In') if count == 18 else nfkc(table['name'])
            nine_name = re.sub(r' (?:COURSE|Course)$', '', nine_name).strip()
            nine_name = ' '.join(w.title() if w.isupper() else w for w in nine_name.split())
            par = integers(rows.get('Par', [])[offset:offset + 9], 3, 6)
            if par is None:
                raise ValueError('invalid par')
            nines.append(dict(name=nine_name, par=par))
            rows_by_nine.append({k: v[offset:offset + 9] for k, v in rows.items()})
    if not 1 <= len(nines) <= 4:
        raise ValueError('nine count must be 1..4')
    names = [n['name'] for n in nines]
    if len(set(names)) != len(names) or not all(valid_text(n, 11) for n in names):
        for i, nine in enumerate(nines, 1):
            nine['name'] = f'Nine {i}'
    si = [integers(r.get('HDCP', []), 1, 9 if len(nines) == 1 else 18) for r in rows_by_nine]
    if all(s is not None and len(set(s)) == 9 for s in si):
        for nine, values in zip(nines, si):
            nine['si'] = values
    tees = {}
    for i, rows in enumerate(rows_by_nine):
        for raw, values in rows.items():
            if raw in ('HOLE', 'Par', 'HDCP', 'N/D'):
                continue
            tee_name = COLORS.get(raw, raw)
            if not valid_text(tee_name, 11):
                report.append(f'{cid}: dropped tee {raw!r}: invalid name')
                continue
            tees.setdefault(tee_name, [None] * len(nines))[i] = integers(values, 1, 999)
    result_tees = []
    for tee_name, yards in tees.items():
        tee = dict(name=tee_name)
        if all(y is not None for y in yards):
            tee['yards'] = yards
        result_tees.append(tee)
    result_tees.sort(key=lambda t: ('yards' not in t, -sum(map(sum, t.get('yards', []))),
                                  PRIORITY.index(t['name']) if t['name'] in PRIORITY else 100, t['name']))
    if len(result_tees) > 6:
        report.append(f'{cid}: dropped tees beyond six: ' + ', '.join(t['name'] for t in result_tees[6:]))
    metadata = dict(id=cid, region=region)
    evaluation = detail.get('total_eval') or {}
    reviews = positive(evaluation.get('peop_total'))
    if reviews:
        rating = scaled(detail.get('eval_score_avg'))
        if rating is not None:
            metadata.update(rating=rating, reviews=reviews)
    scores = [scaled(evaluation.get(k)) for k in SCORES]
    if all(s is not None for s in scores):
        metadata['scores'] = scores
    for key in ('holes', 'par', 'yards'):
        value = positive((detail.get('yardage') or detail.get('distance')) if key == 'yards' else detail.get(key))
        if value:
            metadata[key] = value
    designer = detail.get('designer')
    if isinstance(designer, str) and designer.isascii() and valid_text(designer, 31, '\r\n'):
        metadata['designer'] = designer
    year = str(detail.get('build_time', ''))
    if re.fullmatch(r'[1-9][0-9]{3}', year):
        metadata['opened'] = int(year)
    course = dict(v=2, name=name, maps=f'bg-{cid}', baigolf=metadata, nines=nines)
    if result_tees:
        course['tees'] = result_tees[:6]
    return course, source


def positive(value):
    try:
        number = int(str(value))
        return number if number > 0 else None
    except (ValueError, TypeError):
        return None


def scaled(value):
    try:
        number = Decimal(str(value))
        if not number.is_finite() or not 0 <= number <= 5:
            return None
        return int((number * 10).quantize(Decimal('1'), rounding=ROUND_HALF_UP))
    except InvalidOperation:
        return None


def raw_nine_names(scorecards):
    names = []
    for table in scorecards['list']:
        rows = {nfkc(row['name']): row['values'] for row in table['scorecard']}
        holes = rows.get('HOLE', [])
        count = len(holes) - (1 if holes and str(holes[-1]).upper() == 'TOTAL' else 0)
        names.extend([table.get('name', '')] * (count // 9))
    return names


def image_token(value, group_key=False):
    text = nfkc(value).lower()
    compact = re.sub(r'\s+', '', text)
    compass = set()
    for words, token in [('north', 'N'), ('south', 'S'), ('east', 'E'), ('west', 'W'),
                         ('mid middle center centre', 'M')]:
        if any(word in compact for word in words.split()):
            compass.add(token)
    singles = [compact] if group_key else re.findall(r'(?<![a-z])([nsewm])(?![a-z])', text)
    compass.update(value.upper() for value in singles if value in 'nsewm' and len(value) == 1)
    chinese = {'北': 'N', '南': 'S', '东': 'E', '西': 'W', '中': 'M'}
    if compact.rstrip('-_号洞场区') in chinese:
        compass.add(chinese[compact.rstrip('-_号洞场区')])
    if compass:
        return next(iter(compass)) if len(compass) == 1 else '?'
    letters = set(re.findall(r'^[a-d]$', compact) if group_key else
                  re.findall(r'(?<![a-z])([a-d])(?![a-z])', text))
    if letters:
        return next(iter(letters)).upper() if len(letters) == 1 else '?'
    halves = set()
    if 'out' in compact or compact.rstrip('-_号洞场区') == '前':
        halves.add('OUT')
    if re.search(r'(?<![a-z])in(?![a-z])', text) or compact == 'in' or compact.rstrip('-_号洞场区') == '后':
        halves.add('IN')
    return next(iter(halves)) if len(halves) == 1 else '?' if halves else None


def image_holes(images, nine_count, report=None, nine_names=None):
    count = nine_count * 9
    groups = {}
    for item in images:
        title = re.sub(r'\s+', '', nfkc(item['images_title']))
        match = re.search(r'(\d+)(?!.*\d)', title)
        if not match:
            continue
        number = int(match[1])
        key = title[:match.start()].removeprefix('第').rstrip('-_号洞场区')
        if not 1 <= number <= count or re.search(r'\d{6,}', key):
            continue
        groups.setdefault(key, []).append((number, item))
    if not groups:
        if report is not None:
            report.append('no maps: no usable numbered titles')
        return {}

    numbers = [set(number for number, _ in items) for items in groups.values()]
    direct = len(groups) == 1 or sum(map(len, numbers)) == len(set().union(*numbers))
    assignments = {}
    reason = None
    if not direct:
        if len(groups) <= nine_count and all(max(values) <= 9 for values in numbers):
            width = 1
        elif 2 * len(groups) <= nine_count and all(9 < max(values) <= 18 for values in numbers):
            width = 2
        else:
            width = 0
        tokens = {key: image_token(key, group_key=True) for key in groups}
        if '' in tokens and all(token in ('A', 'B', 'C', 'D') for key, token in tokens.items() if key):
            tokens[''] = 'A'
        names = [image_token(name) for name in (nine_names if nine_names is not None else [''] * nine_count)]
        if not width or len(names) != nine_count or any(token in (None, '?') for token in tokens.values()):
            reason = 'ambiguous image numbering or group tokens'
        else:
            occupied = set()
            letters_only = not any(names) and all(token in ('A', 'B', 'C', 'D') for token in tokens.values())
            if letters_only and report is not None:
                report.append(f'letters-only fallback: {tokens!r}')
            for key, token in tokens.items():
                if letters_only:
                    first = (ord(token) - ord('A')) * width
                    positions = list(range(first, first + width))
                else:
                    positions = [index for index, name in enumerate(names) if name == token]
                if len(positions) != width or any(index >= nine_count or index in occupied for index in positions):
                    reason = 'image groups do not uniquely match scorecard nines'
                    break
                assignments[key] = positions
                occupied.update(positions)
    if reason:
        if report is not None:
            report.append(f'no maps: {reason}')
        return {}

    claims = {}
    for key, items in groups.items():
        for number, item in items:
            if direct:
                hole = number
            else:
                positions = assignments[key]
                hole = 9 * positions[(number - 1) // 9] + (number - 1) % 9 + 1
            if hole <= count:
                claims.setdefault(hole, []).append(item)
    if report is not None:
        duplicates = sorted(hole for hole, items in claims.items() if len(items) > 1)
        if duplicates:
            report.append(f'dropped duplicate holes: {duplicates}')
    return {hole: items[0] for hole, items in claims.items() if len(items) == 1}


def hsv_tone(hue, saturation, value):
    """Apply §41.3 classes in order, rounding the resulting tone to a byte."""
    if saturation < WHITE_SATURATION_MAX and value > WHITE_VALUE_MIN:
        return WHITE_TONE
    if WATER_HUE_MIN < hue < WATER_HUE_MAX and saturation > WATER_SATURATION_MIN:
        return WATER_TONE
    if SAND_HUE_MIN < hue < SAND_HUE_MAX and value > SAND_VALUE_MIN:
        return SAND_TONE
    if GRASS_HUE_MIN <= hue <= GRASS_HUE_MAX:
        return round(GRASS_TONE_BASE + GRASS_TONE_RANGE * value ** GRASS_VALUE_EXPONENT)
    return round(OTHER_TONE_RANGE * value)


def tone_map(image):
    Image = pillow()
    rgb = image.convert('RGB')
    tones = bytes(hsv_tone(*colorsys.rgb_to_hsv(red / 255, green / 255, blue / 255))
                  for red, green, blue in struct.iter_unpack('BBB', rgb.tobytes()))
    return Image.frombytes('L', rgb.size, tones)


def bitmap(source, bpp):
    Image = pillow()
    with Image.open(source) as original:
        image = tone_map(original)
    scale = min(480 / image.width, 640 / image.height)
    image = image.resize((max(1, round(image.width * scale)), max(1, round(image.height * scale))), Image.Resampling.LANCZOS)
    width, height = image.size
    if bpp == 2:
        palette_image = Image.new('P', (1, 1))
        palette_image.putpalette([v for level in (0, 85, 170, 255) for v in (level,) * 3] + [255] * (252 * 3))
        image = image.convert('RGB').quantize(palette=palette_image, dither=Image.Dither.FLOYDSTEINBERG)
        # Duplicate white palette entries may be chosen; all map to level 3.
        pixels = bytes(min(v, 3) for v in image.tobytes())
    else:
        pixels = image.tobytes()
    stride = ((width * bpp + 31) // 32) * 4
    rows = bytearray(stride * height)
    for y in range(height):
        for x in range(width):
            value = pixels[y * width + x]
            offset = (height - y - 1) * stride
            if bpp == 2:
                rows[offset + x // 4] |= value << (6 - 2 * (x % 4))
            else:
                rows[offset + x] = value
    levels = (0, 85, 170, 255) if bpp == 2 else range(256)
    palette = bytes(v for level in levels for v in (level, level, level, 0))
    start = 54 + len(palette)
    return (struct.pack('<2sIHHI', b'BM', start + len(rows), 0, 0, start) +
            struct.pack('<IiiHHIIiiII', 40, width, height, 1, bpp, 0, len(rows), 0, 0, len(levels), 0) + palette + rows)


def index_record(course, region_idx, has_maps):
    meta = course['baigolf']
    rating = f"{meta['rating']:02}" if 'rating' in meta else '--'
    par = sum(sum(n['par']) for n in course['nines'][:2])
    record = (f"{meta['id']:08}|{region_idx:02}|{len(course['nines'])}|{par:03}|{rating}|"
              f"{min(meta.get('reviews', 0), 99999):05}|{'Y' if has_maps else 'N'}|{course['name']:<40}|{'':25}\n").encode('ascii')
    if len(record) != 96:
        raise ValueError('index field overflow')
    return record


def significant(name):
    return set(re.findall(r'[a-z]+', name.lower())) - {'golf', 'club', 'country', 'and', 'the', 'cc', 'gc', 'intl'}


def export(cache, out, bpp=2, force=False):
    cache, out = external_path(cache), external_path(out)
    pillow()
    catalog = out / 'golf/catalog'
    if catalog.exists() and not force:
        raise ValueError(f'{catalog} already exists; use --force')
    entries = sorted(read_json(cache / 'sitemap.json'), key=lambda e: e['id'])
    unknown = {e['region_zh'] for e in entries} - REGIONS.keys()
    if unknown:
        raise ValueError(f'unknown regions: {sorted(unknown)}')
    report, courses, used = [], [], set()
    for entry in entries:
        cid = entry['id']
        # Only cached selections are exported; the sitemap may contain all China courses.
        if not (cache / 'detail' / f'{cid}.json').exists():
            continue
        try:
            if not 1 <= cid <= 99999999:
                raise ValueError('id must fit eight digits')
            detail = payload(read_json(cache / 'detail' / f'{cid}.json'))
            scorecards = payload(read_json(cache / 'scorecards' / f'{cid}.json'))
            course, source = build_course(entry, detail, scorecards, report)
            base, suffix = course['name'], 2
            while course['name'].casefold() in used:
                tail = f' {suffix}'
                course['name'] = shorten(base, 39 - len(tail)) + tail
                suffix += 1
            used.add(course['name'].casefold())
            if course['name'] != source:
                report.append(f"{cid}: name {source!r} -> {course['name']!r}")
            image_report = []
            mapping = image_holes(detail.get('ball_path_img_list', []), len(course['nines']), image_report,
                                  raw_nine_names(scorecards))
            report.extend(f'{cid}: {message}' for message in image_report)
            courses.append((course, mapping))
        except (ValueError, KeyError, TypeError, OSError) as error:
            report.append(f'{cid}: skipped: {error}')
    if catalog.exists():
        shutil.rmtree(catalog)
    catalog.mkdir(parents=True)
    regions = sorted({c['baigolf']['region'] for c, _ in courses})
    region_ids = {r: i for i, r in enumerate(regions)}
    courses.sort(key=lambda pair: (region_ids[pair[0]['baigolf']['region']], pair[0]['name'].casefold(), pair[0]['baigolf']['id']))
    records = []
    owners = []
    for path in sorted((out / 'golf/courses').glob('*.json')):
        try:
            owners.append((path.name, read_json(path)['name']))
        except (ValueError, KeyError, OSError):
            pass
    for course, mapping in courses:
        cid = course['baigolf']['id']
        map_dir = out / 'golf/maps' / course['maps']
        # Clear only this catalog id's maps, including stale holes on a forced export.
        if map_dir.exists():
            shutil.rmtree(map_dir)
        exported = 0
        for hole, item in sorted(mapping.items()):
            source = cache / 'images' / str(cid) / f"{item['id']}.png"
            try:
                atomic_write(map_dir / f'hole-{hole}.bmp', bitmap(source, bpp))
                exported += 1
            except (OSError, ValueError) as error:
                report.append(f'{cid}: map {hole} unavailable: {error}')
        expected = 9 * len(course['nines'])
        report.append(f'{cid}: maps resolved {len(mapping)}/{expected}; exported {exported}/{expected}')
        write_json(catalog / 'courses' / f'bg-{cid}.json', course)
        records.append(index_record(course, region_ids[course['baigolf']['region']], exported > 0))
        for filename, owner_name in owners:
            if len(significant(owner_name) & significant(course['name'])) >= 2:
                report.append(f'{cid}: suggested owner duplicate {filename}: {owner_name}')
    atomic_write(catalog / 'index.dat', b''.join(records))
    # Cache mtime fixes the date across repeated exports of unchanged input.
    date_source = cache / 'sitemap.html'
    if not date_source.exists():
        date_source = cache / 'sitemap.json'
    date = datetime.datetime.fromtimestamp(date_source.stat().st_mtime, datetime.timezone.utc).date()
    lines = [f'# golf-catalog 1 {date} {len(regions)} {len(courses)}']
    first = 0
    for region in regions:
        count = sum(c['baigolf']['region'] == region for c, _ in courses)
        lines.append(f'{region_ids[region]:02}|{region}|{count}|{first}')
        first += count
    atomic_write(catalog / 'regions.txt', ('\n'.join(lines) + '\n').encode())
    atomic_write(cache / 'export-report.txt', ('\n'.join(report) + ('\n' if report else '')).encode())
    return [c for c, _ in courses]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', default=DEFAULT_CACHE)
    parser.add_argument('--out', required=True)
    parser.add_argument('--bpp', type=int, choices=(2, 8), default=2)
    parser.add_argument('--force', action='store_true')
    args = parser.parse_args()
    try:
        courses = export(args.cache, args.out, args.bpp, args.force)
        print(f'Exported {len(courses)} courses; report: {args.cache}/export-report.txt')
    except (ValueError, OSError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
