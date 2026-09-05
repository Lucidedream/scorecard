"""Parse full downloads independently of the firmware's formatter."""
import csv
import io
import json
import subprocess
import sys
from html.parser import HTMLParser

for kind in ('detailed', 'summary'):
    def report(fmt):
        return subprocess.check_output([sys.argv[1], str(fmt), kind]).decode('utf-8')

    data = json.loads(report(2))
    assert data['gross_strokes'] == 8
    assert data['played_par'] == 4
    assert data['penalty_strokes'] == 2
    assert data['putts'] == 2 and data['in100'] == 3
    assert data['course'] == '=Golf, "山" <&>'
    rows = list(csv.DictReader(io.StringIO(report(1))))
    assert all(None not in row and None not in row.values() for row in rows)
    assert sum(int(row['gross_strokes'] or 0) for row in rows) == data['gross_strokes']
    assert int(rows[0]['par']) == 4
    assert rows[0]['course'] == "'" + data['course']
    assert 'Gross strokes: 8' in report(0)
    assert report(0).count('Inside 100 includes putts.') == 1
    HTMLParser().feed(report(3))
    assert '&lt;&amp;&gt;' in report(3)
    if kind == 'detailed':
        assert len(data['holes']) == len(rows) == 9
        assert data['holes'][0]['penalty_events'] == [{'field': 'out100', 'kind': 'ob'}]
        assert data['holes'][1]['gross_strokes'] is None
        assert rows[0]['penalty_events'] == '2:1'
    else:
        assert not data['holes'] and len(rows) == 1
        assert data['holes_entered'] is None
print('JSON, CSV, TXT, HTML: detailed and summary fixtures passed')
