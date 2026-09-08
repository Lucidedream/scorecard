"""Expose the real English golf UI copy to host tests without Arduino/I18n.

Emits every STR_* key from english.yaml as a constexpr char[] plus a `tr(id)` macro
that is the identity, so GolfReviewFormat.cpp's tr(STR_GOLF_*) calls compile and
return the real strings.
"""
import json
import pathlib
import sys

lines = ['#pragma once', '#define tr(id) id']
for line in pathlib.Path(sys.argv[1]).read_text().splitlines():
    if not line.startswith('STR_'):
        continue
    key, value = line.split(':', 1)
    # english.yaml uses JSON-compatible double-quoted scalars.
    text = json.loads(value.strip())
    lines.append(f'inline constexpr char {key}[] = {json.dumps(text, ensure_ascii=False)};')
pathlib.Path(sys.argv[2]).write_text('\n'.join(lines) + '\n')
