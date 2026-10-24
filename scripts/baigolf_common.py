"""Shared host-only catalog I/O and provenance guards (§41)."""
import json
import os
import subprocess
from pathlib import Path

DEFAULT_CACHE = Path.home() / 'golf-catalog-cache'


def external_path(value):
    path = Path(value).expanduser().resolve()
    ancestor = path
    while not ancestor.exists():
        ancestor = ancestor.parent
    result = subprocess.run(['git', '-C', str(ancestor), 'rev-parse', '--is-inside-work-tree'],
                            capture_output=True, text=True)
    if result.returncode == 0 and result.stdout.strip() == 'true':
        raise ValueError(f'{path}: scraped data must stay outside git work trees (personal-use provenance)')
    return path


def atomic_write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.tmp')
    try:
        temporary.write_bytes(data)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def write_json(path, value):
    atomic_write(path, (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode())


def read_json(path):
    return json.loads(Path(path).read_text())


def payload(response):
    if not isinstance(response, dict):
        raise ValueError('API response must be an object')
    if response.get('succeed') != 1:
        raise ValueError(f"API succeed={response.get('succeed')!r}")
    if 'data' not in response:
        raise ValueError('API response missing data')
    return response['data']
