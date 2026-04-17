#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
scarfnet-log-charts.py — Timeline charts from Scarfnet serial logs

Produces a self-contained HTML file with five Chart.js charts:

  1. Time-sync offset over time       — [MESH] Adjusted time … Offset = N (µs)
  2. SWARM arrival deltas per node    — raw (dashed) + smoothed (solid) per peer
  3. Connection events timeline       — NEW / DROP / CHANGED / node-count swimlane
  4. Large offset spikes              — Chart 1 data, markers where |offset| > 100 000 µs
  5. Sent heartbeats                  — [SND] events; pattern label + inter-heartbeat gap

Usage:
    uv run tools/scarfnet-log-charts.py logs/session.txt
    uv run tools/scarfnet-log-charts.py logs/2026-04-15-log*.txt -o charts.html
"""

import re
import sys
import json
import argparse
import colorsys
from pathlib import Path
from dataclasses import dataclass, field
from typing import Any

# ---------------------------------------------------------------------------
# Regex
# ---------------------------------------------------------------------------
RE_ADJ      = re.compile(r'\[MESH\] Adjusted time (\d+)ms\. Offset = (-?\d+)')
RE_SND      = re.compile(r'\[SND\] (\{.+\})')
RE_SWARM_D  = re.compile(r'\[SWARM\] node (\d+) delta: raw=(-?\d+)ms smoothed=(-?\d+)ms')
RE_SWARM_F  = re.compile(r'\[SWARM\] node (\d+) first delta: (-?\d+)ms')
RE_NEW      = re.compile(r'\[MESH\]\[NEW node (\d+)\]')
RE_DROP     = re.compile(r'\[MESH\]\[DROP node (\d+)\]')
RE_CHANGED  = re.compile(r'\[MESH\] Changed connections')
RE_CONNLIST = re.compile(r'Connection list \((\d+) nodes\)')

# Anchor for deriving receive time from this node's clock
RE_ANCHOR   = re.compile(r'(?:\[MESH\] Adjusted time (\d+)ms)|(?:\[SND\].*"currentTimeMs"\s*:\s*(\d+))')

# ---------------------------------------------------------------------------
# Timestamp interpolation (same approach as add-timestamps.py)
# ---------------------------------------------------------------------------

def build_anchor_table(lines: list[str]) -> list[tuple[int, int]]:
    anchors = []
    for i, line in enumerate(lines):
        m = RE_ANCHOR.search(line)
        if m:
            t = int(m.group(1) or m.group(2))
            anchors.append((i, t))
    return anchors


def detect_reboot(anchors: list[tuple[int, int]]) -> int | None:
    for i in range(1, len(anchors)):
        if anchors[i][1] < anchors[i - 1][1] - 60_000:
            return i
    return None


def make_time_fn(lines: list[str]):
    """Return a function line_index → float ms (session-relative, stitched across reboots)."""
    raw = build_anchor_table(lines)
    if not raw:
        return lambda i: float(i)

    reboot = detect_reboot(raw)
    if reboot is not None:
        pre  = raw[:reboot]
        post = raw[reboot:]
        offset = pre[-1][1] + 500 - post[0][1]
        anchors = pre + [(li, t + offset) for li, t in post]
    else:
        anchors = raw

    n = len(anchors)

    def lerp(i0, t0, i1, t1, i):
        if i1 == i0:
            return float(t0)
        return t0 + (t1 - t0) * (i - i0) / (i1 - i0)

    def time_of(i: int) -> float:
        lo = -1
        for k in range(n):
            if anchors[k][0] <= i:
                lo = k
            else:
                break
        if lo == -1:
            if n >= 2:
                return lerp(anchors[0][0], anchors[0][1], anchors[1][0], anchors[1][1], i)
            return float(anchors[0][1])
        if lo == n - 1:
            if n >= 2:
                return lerp(anchors[-2][0], anchors[-2][1], anchors[-1][0], anchors[-1][1], i)
            return float(anchors[-1][1])
        return lerp(anchors[lo][0], anchors[lo][1], anchors[lo+1][0], anchors[lo+1][1], i)

    # Normalize to session start
    t0 = time_of(anchors[0][0])
    return lambda i: time_of(i) - t0


# ---------------------------------------------------------------------------
# Data extraction
# ---------------------------------------------------------------------------

@dataclass
class LogData:
    name: str
    # Chart 1 & 4: time-sync offsets
    offsets: list[dict] = field(default_factory=list)   # {x, y} x=ms, y=offset µs
    # Chart 2: swarm deltas
    swarm: dict[str, list[dict]] = field(default_factory=dict)  # node → [{x, raw, smoothed}]
    # Chart 3: connection events
    conn_events: list[dict] = field(default_factory=list)  # {x, type, node, count}
    # Chart 5: sent heartbeats
    heartbeats: list[dict] = field(default_factory=list)   # {x, pattern, randomizer, gap}


def parse_log(path: Path) -> LogData:
    lines = path.read_text(errors='replace').splitlines()
    time_of = make_time_fn(lines)
    data = LogData(name=path.name)

    last_snd_t: float | None = None

    for i, raw in enumerate(lines):
        t = time_of(i)

        # Chart 1 & 4: offset
        m = RE_ADJ.search(raw)
        if m:
            data.offsets.append({'x': round(t), 'y': int(m.group(2))})
            continue

        # Chart 2: swarm deltas
        m = RE_SWARM_D.search(raw)
        if m:
            node = m.group(1)
            entry = {'x': round(t), 'raw': int(m.group(2)), 'smoothed': int(m.group(3))}
            data.swarm.setdefault(node, []).append(entry)
            continue

        m = RE_SWARM_F.search(raw)
        if m:
            node = m.group(1)
            entry = {'x': round(t), 'raw': int(m.group(2)), 'smoothed': 0}
            data.swarm.setdefault(node, []).append(entry)
            continue

        # Chart 3: connection events
        m = RE_NEW.search(raw)
        if m:
            data.conn_events.append({'x': round(t), 'type': 'NEW', 'node': m.group(1), 'count': None})
            continue

        m = RE_DROP.search(raw)
        if m:
            data.conn_events.append({'x': round(t), 'type': 'DROP', 'node': m.group(1), 'count': None})
            continue

        if RE_CHANGED.search(raw):
            data.conn_events.append({'x': round(t), 'type': 'CHANGED', 'node': None, 'count': None})
            continue

        m = RE_CONNLIST.search(raw)
        if m:
            if data.conn_events:
                data.conn_events[-1]['count'] = int(m.group(1))
            continue

        # Chart 5: heartbeats
        m = RE_SND.search(raw)
        if m:
            try:
                doc = json.loads(m.group(1))
                gap = round(t - last_snd_t) if last_snd_t is not None else None
                data.heartbeats.append({
                    'x': round(t),
                    'pattern': doc.get('pattern', '?'),
                    'randomizer': doc.get('randomizer', 0),
                    'changeIndex': doc.get('changeIndex', 0),
                    'gap': gap,
                })
                last_snd_t = t
            except json.JSONDecodeError:
                pass
            continue

    return data


# ---------------------------------------------------------------------------
# Color helpers
# ---------------------------------------------------------------------------

def hsl(h: float, s: float = 0.7, l: float = 0.5) -> str:
    r, g, b = colorsys.hls_to_rgb(h, l, s)
    return f'rgba({int(r*255)},{int(g*255)},{int(b*255)},1)'


def node_color(node: str, alpha: float = 1.0) -> str:
    h = (int(node) % 997) / 997
    r, g, b = colorsys.hls_to_rgb(h, 0.5, 0.65)
    return f'rgba({int(r*255)},{int(g*255)},{int(b*255)},{alpha})'


FILE_COLORS = [
    'rgba(59,130,246,1)',   # blue
    'rgba(239,68,68,1)',    # red
    'rgba(34,197,94,1)',    # green
    'rgba(251,146,60,1)',   # orange
    'rgba(168,85,247,1)',   # purple
    'rgba(20,184,166,1)',   # teal
    'rgba(236,72,153,1)',   # pink
    'rgba(234,179,8,1)',    # yellow
]


# ---------------------------------------------------------------------------
# HTML / Chart.js generation
# ---------------------------------------------------------------------------

CHART_JS_CDN = 'https://cdn.jsdelivr.net/npm/chart.js@4/dist/chart.umd.min.js'

HTML_TEMPLATE = """\
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Scarfnet Log Charts</title>
<style>
  body {{ font-family: system-ui, sans-serif; background: #0f172a; color: #e2e8f0; margin: 0; padding: 1rem 2rem; }}
  h1   {{ font-size: 1.4rem; margin-bottom: 0.2rem; }}
  .subtitle {{ color: #94a3b8; font-size: 0.85rem; margin-bottom: 2rem; }}
  .chart-wrap {{ background: #1e293b; border-radius: 10px; padding: 1rem 1.5rem 1.5rem; margin-bottom: 2rem; }}
  h2   {{ font-size: 1rem; color: #7dd3fc; margin: 0 0 0.8rem; }}
  canvas {{ max-height: 340px; }}
  .legend-note {{ color: #64748b; font-size: 0.78rem; margin-top: 0.5rem; }}
</style>
</head>
<body>
<h1>Scarfnet Log Charts</h1>
<div class="subtitle">Files: {files_list}</div>

<div class="chart-wrap">
  <h2>1 — Time-sync offset over time</h2>
  <canvas id="c1"></canvas>
  <div class="legend-note">Y axis: clock offset in µs applied by painlessMesh. Positive = clock advanced, negative = clock retarded.</div>
</div>

<div class="chart-wrap">
  <h2>2 — SWARM arrival deltas per node</h2>
  <canvas id="c2"></canvas>
  <div class="legend-note">Solid = EMA-smoothed; dashed = raw sample. One colour per peer node. Y axis: receiverTimeMs − senderTimeMs (ms).</div>
</div>

<div class="chart-wrap">
  <h2>3 — Connection events</h2>
  <canvas id="c3"></canvas>
  <div class="legend-note">Triangles = NEW, circles = DROP, diamonds = CHANGED. Y = node count at event (when known).</div>
</div>

<div class="chart-wrap">
  <h2>4 — Large time-sync offset spikes (|offset| &gt; 100 000 µs)</h2>
  <canvas id="c4"></canvas>
  <div class="legend-note">Same data as Chart 1, with the spike threshold shown as a dashed reference line.</div>
</div>

<div class="chart-wrap">
  <h2>5 — Sent heartbeats &amp; inter-heartbeat gap</h2>
  <canvas id="c5"></canvas>
  <div class="legend-note">Bars = gap since previous heartbeat (ms). Tooltip shows pattern + randomizer.</div>
</div>

<script src="{chartjs_cdn}"></script>
<script>
const DATA = {data_json};

// ── Shared options ──────────────────────────────────────────────────────────
Chart.defaults.color = '#94a3b8';
Chart.defaults.borderColor = '#334155';

const tickX = {{
  ticks: {{
    callback: v => (v/1000).toFixed(0)+'s',
    maxTicksLimit: 15,
    color: '#94a3b8',
  }},
  title: {{ display: true, text: 'Session time (s)', color: '#64748b' }},
  grid: {{ color: '#1e3a5f' }},
}};

function makeTooltip(extra) {{
  return {{
    callbacks: {{
      title: items => (items[0].parsed.x / 1000).toFixed(2) + 's',
      ...extra,
    }},
  }};
}}

// ── Chart 1: offsets ────────────────────────────────────────────────────────
const FILE_COLORS_C1 = {file_colors_js};
new Chart(document.getElementById('c1'), {{
  type: 'line',
  data: {{
    datasets: DATA.offsets.map((d, fi) => ({{
      label: d.name,
      data: d.points,
      borderColor: FILE_COLORS_C1[fi % FILE_COLORS_C1.length],
      backgroundColor: 'transparent',
      borderWidth: 1.2,
      pointRadius: 0,
      tension: 0.1,
    }})),
  }},
  options: {{
    animation: false,
    parsing: false,
    scales: {{
      x: {{...tickX}},
      y: {{
        title: {{ display: true, text: 'Offset (µs)', color: '#64748b' }},
        grid: {{ color: '#1e3a5f' }},
      }},
    }},
    plugins: {{ tooltip: makeTooltip({{ label: i => i.dataset.label + ': ' + i.parsed.y.toLocaleString() + ' µs' }}) }},
  }},
}});

// ── Chart 2: swarm deltas ──────────────────────────────────────────────────
(function() {{
  const FILE_COLORS = {file_colors_js};
  const ds = [];
  for (const [node, series] of Object.entries(DATA.swarm)) {{
    const col = series.color;
    ds.push({{
      label: 'node ' + node + ' smoothed',
      data: series.smoothed,
      borderColor: col,
      backgroundColor: 'transparent',
      borderWidth: 1.5,
      pointRadius: 0,
      tension: 0.2,
    }});
    ds.push({{
      label: 'node ' + node + ' raw',
      data: series.raw,
      borderColor: col,
      backgroundColor: 'transparent',
      borderWidth: 0.8,
      borderDash: [4, 3],
      pointRadius: 0,
      tension: 0,
    }});
  }}
  new Chart(document.getElementById('c2'), {{
    type: 'line',
    data: {{ datasets: ds }},
    options: {{
      animation: false,
      parsing: false,
      scales: {{
        x: {{...tickX}},
        y: {{
          title: {{ display: true, text: 'Delta (ms)', color: '#64748b' }},
          grid: {{ color: '#1e3a5f' }},
        }},
      }},
      plugins: {{
        legend: {{ display: false }},
        tooltip: makeTooltip({{ label: i => i.dataset.label + ': ' + i.parsed.y + ' ms' }}),
      }},
    }},
  }});
}})();

// ── Chart 3: connection events ─────────────────────────────────────────────
(function() {{
  const colNew     = 'rgba(34,197,94,0.9)';
  const colDrop    = 'rgba(239,68,68,0.9)';
  const colChanged = 'rgba(251,146,60,0.9)';

  function eventsToScatter(events, type) {{
    return events
      .filter(e => e.type === type)
      .map(e => ({{ x: e.x, y: e.count ?? 0, node: e.node }}));
  }}

  const allEvents = DATA.conn_events.flatMap(d => d.events);
  const ds = [
    {{
      label: 'NEW',
      data: eventsToScatter(allEvents, 'NEW'),
      backgroundColor: colNew,
      pointStyle: 'triangle',
      pointRadius: 8,
    }},
    {{
      label: 'DROP',
      data: eventsToScatter(allEvents, 'DROP'),
      backgroundColor: colDrop,
      pointStyle: 'circle',
      pointRadius: 6,
    }},
    {{
      label: 'CHANGED',
      data: eventsToScatter(allEvents, 'CHANGED'),
      backgroundColor: colChanged,
      pointStyle: 'rectRot',
      pointRadius: 7,
    }},
  ];

  new Chart(document.getElementById('c3'), {{
    type: 'scatter',
    data: {{ datasets: ds }},
    options: {{
      animation: false,
      scales: {{
        x: {{...tickX}},
        y: {{
          title: {{ display: true, text: 'Node count', color: '#64748b' }},
          grid: {{ color: '#1e3a5f' }},
          ticks: {{ stepSize: 1 }},
        }},
      }},
      plugins: {{
        tooltip: makeTooltip({{
          label: i => {{
            const d = i.raw;
            return i.dataset.label + (d.node ? ' ' + d.node : '') + (d.y ? '  (' + d.y + ' nodes)' : '');
          }},
        }}),
      }},
    }},
  }});
}})();

// ── Chart 4: large offset spikes ──────────────────────────────────────────
(function() {{
  const FILE_COLORS = {file_colors_js};
  const THRESH = 100000;
  const ds = DATA.offsets.map((d, fi) => {{
    const col = FILE_COLORS[fi % FILE_COLORS.length];
    return {{
      label: d.name,
      data: d.points.filter(p => Math.abs(p.y) > THRESH),
      borderColor: col,
      backgroundColor: col.replace(',1)', ',0.4)'),
      borderWidth: 0,
      pointRadius: 5,
      showLine: false,
    }};
  }});

  ds.push({{
    label: '+100 000 µs',
    data: [{{ x: DATA.x_min, y: THRESH }}, {{ x: DATA.x_max, y: THRESH }}],
    borderColor: 'rgba(148,163,184,0.4)',
    borderDash: [6, 4],
    borderWidth: 1,
    pointRadius: 0,
    showLine: true,
  }});
  ds.push({{
    label: '−100 000 µs',
    data: [{{ x: DATA.x_min, y: -THRESH }}, {{ x: DATA.x_max, y: -THRESH }}],
    borderColor: 'rgba(148,163,184,0.4)',
    borderDash: [6, 4],
    borderWidth: 1,
    pointRadius: 0,
    showLine: true,
  }});

  new Chart(document.getElementById('c4'), {{
    type: 'scatter',
    data: {{ datasets: ds }},
    options: {{
      animation: false,
      scales: {{
        x: {{...tickX}},
        y: {{
          title: {{ display: true, text: 'Offset (µs)', color: '#64748b' }},
          grid: {{ color: '#1e3a5f' }},
        }},
      }},
      plugins: {{ tooltip: makeTooltip({{ label: i => i.dataset.label + ': ' + i.parsed.y.toLocaleString() + ' µs' }}) }},
    }},
  }});
}})();

// ── Chart 5: heartbeat gaps ────────────────────────────────────────────────
(function() {{
  const FILE_COLORS = {file_colors_js};
  const GAP_WARN = 7000;
  const ds = DATA.heartbeats.map((d, fi) => {{
    const col = FILE_COLORS[fi % FILE_COLORS.length];
    return {{
      label: d.name,
      data: d.points,
      backgroundColor: d.points.map(p =>
        p.gap > GAP_WARN ? 'rgba(239,68,68,0.8)' : col.replace(',1)', ',0.6)')),
      pointRadius: 5,
      pointStyle: 'circle',
    }};
  }});

  new Chart(document.getElementById('c5'), {{
    type: 'scatter',
    data: {{ datasets: ds }},
    options: {{
      animation: false,
      parsing: false,
      scales: {{
        x: {{...tickX}},
        y: {{
          title: {{ display: true, text: 'Gap since prev heartbeat (ms)', color: '#64748b' }},
          grid: {{ color: '#1e3a5f' }},
        }},
      }},
      plugins: {{
        tooltip: makeTooltip({{
          label: i => {{
            const p = i.raw;
            return `gap=${{p.gap}}ms  pattern=${{p.pattern}}  rnd=${{p.randomizer}}  idx=${{p.changeIndex}}`;
          }},
        }}),
      }},
    }},
  }});
}})();

</script>
</body>
</html>
"""


# ---------------------------------------------------------------------------
# Assemble JSON payload for the template
# ---------------------------------------------------------------------------

def build_payload(logs: list[LogData]) -> dict[str, Any]:
    # Offsets
    offsets = [
        {'name': lg.name, 'points': lg.offsets}
        for lg in logs
    ]

    # Swarm: merge all nodes across all logs
    swarm: dict[str, dict] = {}
    for lg in logs:
        for node, series in lg.swarm.items():
            if node not in swarm:
                swarm[node] = {
                    'color': node_color(node),
                    'smoothed': [],
                    'raw': [],
                }
            swarm[node]['smoothed'].extend({'x': p['x'], 'y': p['smoothed']} for p in series)
            swarm[node]['raw'].extend({'x': p['x'], 'y': p['raw']} for p in series)

    # Conn events
    conn_events = [{'name': lg.name, 'events': lg.conn_events} for lg in logs]

    # Heartbeats
    heartbeats = [
        {
            'name': lg.name,
            'points': [
                {'x': h['x'], 'y': h['gap'] or 0,
                 'gap': h['gap'] or 0,
                 'pattern': h['pattern'],
                 'randomizer': h['randomizer'],
                 'changeIndex': h['changeIndex']}
                for h in lg.heartbeats
                if h['gap'] is not None
            ],
        }
        for lg in logs
    ]

    # X range for reference lines
    all_x = [p['x'] for lg in logs for p in lg.offsets]
    x_min = min(all_x) if all_x else 0
    x_max = max(all_x) if all_x else 1

    return {
        'offsets': offsets,
        'swarm': swarm,
        'conn_events': conn_events,
        'heartbeats': heartbeats,
        'x_min': x_min,
        'x_max': x_max,
    }


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('logs', nargs='+', type=Path, help='Log file(s)')
    parser.add_argument('-o', '--output', type=Path, default=None)
    args = parser.parse_args()

    all_logs = [parse_log(p) for p in args.logs]

    if args.output is None:
        stem = args.logs[0].stem if len(args.logs) == 1 else 'scarfnet-logs'
        args.output = args.logs[0].parent / (stem + '-charts.html')

    payload = build_payload(all_logs)

    file_colors_list = FILE_COLORS
    html = HTML_TEMPLATE.format(
        files_list=', '.join(p.name for p in args.logs),
        data_json=json.dumps(payload, separators=(',', ':')),
        chartjs_cdn=CHART_JS_CDN,
        file_colors_js=json.dumps(file_colors_list),
    )

    args.output.write_text(html, encoding='utf-8')
    print(f'Written {args.output}  ({args.output.stat().st_size // 1024} KB)')


if __name__ == '__main__':
    main()
