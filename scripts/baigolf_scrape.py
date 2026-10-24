#!/usr/bin/env python3
"""Polite, resumable personal-use baigolf cache builder (§41.1–2)."""
import argparse
import datetime
from html.parser import HTMLParser
import json
import re
import time
import urllib.error
import urllib.parse
import urllib.request

from baigolf_common import DEFAULT_CACHE, atomic_write, external_path, payload, read_json, write_json

BASE = 'https://www.baigolf.com/'


class SitemapParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.heading = None
        self.text = []
        self.country = ''
        self.section = ''
        self.region = ''
        self.link = None
        self.courses = []

    def handle_starttag(self, tag, attrs):
        if tag in ('h1', 'h2', 'h3'):
            if tag in ('h1', 'h2'):
                self.country = ''
                self.region = ''
            self.heading, self.text = tag, []
        if tag == 'a':
            href = dict(attrs).get('href', '')
            match = re.search(r'(?:golf_course_id|golf_id|id)=(\d+)', href)
            self.link = int(match[1]) if match and urllib.parse.urlsplit(href).path.endswith('course.php') else None
            self.text = []

    def handle_data(self, data):
        self.text.append(data)

    def handle_endtag(self, tag):
        text = ''.join(self.text).strip()
        if tag == self.heading:
            if tag == 'h1':
                self.section = text
            elif tag == 'h2':
                self.country = text
            else:
                self.region = text
            self.heading = None
        if tag == 'a' and self.link is not None:
            if self.section == '高尔夫球场' and self.country == '中国' and self.region:
                self.courses.append(dict(id=self.link, region_zh=self.region, name_zh=text))
            self.link = None


class Fetcher:
    def __init__(self, delay):
        self.delay = delay
        self.last = None
        self.downloaded = {}

    def fetch(self, url, path, fields=None, refresh=False, api=False):
        if path.exists() and not refresh:
            if fields is None:
                self.downloaded[url] = path
            return read_json(path) if api else path.read_bytes()
        if fields is None and not refresh and url in self.downloaded:
            data = self.downloaded[url].read_bytes()
            atomic_write(path, data)
            return data
        for attempt in range(3):
            if self.last is not None:
                time.sleep(max(0, self.delay - (time.monotonic() - self.last)))
            request = urllib.request.Request(url, data=urllib.parse.urlencode(fields).encode() if fields else None,
                                             headers={'User-Agent': 'scorecard-catalog/1 (personal use)'})
            try:
                self.last = time.monotonic()
                with urllib.request.urlopen(request, timeout=60) as response:
                    data = response.read()
                if api:
                    try:
                        value = json.loads(data)
                        if not isinstance(value, dict):
                            raise ValueError('API response is not an object')
                    except (ValueError, UnicodeDecodeError):
                        if attempt == 2:
                            raise ValueError(f'non-JSON API reply: {url}; rerun to resume')
                        time.sleep(60)
                        continue
                atomic_write(path, data)
                if fields is None:
                    self.downloaded[url] = path
                return value if api else data
            except urllib.error.HTTPError as error:
                if error.code not in (403, 429) or attempt == 2:
                    raise
                time.sleep(60)
        raise ValueError('request failed; rerun to resume')


def scrape(args):
    cache = external_path(args.cache)
    fetcher = Fetcher(args.delay)
    html = fetcher.fetch(BASE + 'sitemap.php', cache / 'sitemap.html')
    parser = SitemapParser()
    parser.feed(html.decode('utf-8'))
    if not parser.courses:
        raise ValueError('no China courses found in sitemap')
    courses = parser.courses
    if len({c['id'] for c in courses}) != len(courses):
        raise ValueError('duplicate course id in China sitemap span')
    write_json(cache / 'sitemap.json', courses)
    if args.ids:
        wanted = {int(i) for i in args.ids.split(',')}
        missing = wanted - {c['id'] for c in courses}
        if missing:
            raise ValueError(f'ids missing from China sitemap: {sorted(missing)}')
        courses = [c for c in courses if c['id'] in wanted]
    if args.limit is not None:
        courses = courses[:args.limit]
    errors = read_json(cache / 'errors.json') if (cache / 'errors.json').exists() else {}
    for number, course in enumerate(courses, 1):
        cid = course['id']
        try:
            responses = []
            for folder, query in [('detail', 'BaiGolfOrder&_cmd=GET_GOLF_COURSE_DETAIL'),
                                  ('scorecards', 'BaiGolfCourse&_cmd=getScorecards')]:
                fields = {'golf_course_id': cid, 'language': 'zh-cn'}
                if folder == 'detail':
                    fields.update(playDate=str(datetime.date.today() + datetime.timedelta(days=1)), currency_type='CNY')
                response = fetcher.fetch(BASE + 'api.php?_mod=' + query, cache / folder / f'{cid}.json',
                                         fields, args.refresh_details, api=True)
                responses.append(payload(response))
            detail = responses[0]
            if not args.no_images:
                for item in detail.get('ball_path_img_list', []):
                    url = urllib.parse.urlsplit(item['images_url'])
                    query = [(k, v) for k, v in urllib.parse.parse_qsl(url.query) if k != 'x-oss-process']
                    clean = urllib.parse.urlunsplit(url._replace(query=urllib.parse.urlencode(query)))
                    image_id = str(item['id'])
                    if not image_id.isdecimal():
                        raise ValueError('unsafe image id')
                    fetcher.fetch(clean, cache / 'images' / str(cid) / f'{image_id}.png')
            errors.pop(str(cid), None)
            print(f'[{number}/{len(courses)}] {cid} ok', flush=True)
        except ValueError as error:
            if str(error).startswith('API succeed='):
                errors[str(cid)] = str(error)
                write_json(cache / 'errors.json', errors)
                print(f'[{number}/{len(courses)}] {cid} skip {error}', flush=True)
                continue
            print(f'[{number}/{len(courses)}] {cid} error {error}', flush=True)
            raise
        except (OSError, urllib.error.URLError) as error:
            print(f'[{number}/{len(courses)}] {cid} error {error}; rerun to resume', flush=True)
            raise
        write_json(cache / 'errors.json', errors)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', default=DEFAULT_CACHE)
    parser.add_argument('--ids')
    parser.add_argument('--no-images', action='store_true')
    parser.add_argument('--refresh-details', action='store_true')
    parser.add_argument('--delay', type=float, default=2.0)
    parser.add_argument('--limit', type=int)
    args = parser.parse_args()
    if not args.delay >= 1.0 or (args.limit is not None and args.limit < 1):
        parser.error('delay must be at least 1.0 and limit positive')
    try:
        scrape(args)
    except (ValueError, OSError, urllib.error.URLError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
