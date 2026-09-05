"""Expose the real English export copy to host tests without Arduino/I18n."""
import json
import pathlib
import sys

lines = ['#pragma once', '#define tr(id) id']
for line in pathlib.Path(sys.argv[1]).read_text().splitlines():
    if line.startswith('STR_GOLF_EXPORT_'):
        key, value = line.split(':', 1)
        # Export copy uses JSON-compatible quoted YAML scalars.
        text = json.loads(value.strip())
        lines.append(f'inline constexpr char {key}[] = {json.dumps(text, ensure_ascii=False)};')
pathlib.Path(sys.argv[2]).write_text('\n'.join(lines) + '\n')
