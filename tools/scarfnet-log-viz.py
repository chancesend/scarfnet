#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
scarfnet-log-viz.py — Parse Scarfnet firmware logs → OTLP JSON traces

Produces an OpenTelemetry trace file that can be imported into Jaeger,
Grafana Tempo, Honeycomb, or any other OTel-compatible trace viewer.
Each log file becomes a separate service (resource). Events are grouped
into lanes — crash, topology, time_sync, pattern, memory, swarm — each
rendered as a span tree inside the trace.

Handles both log formats:
  New (timestamped): [T+12345] [MESH] Adjusted time ...
  Old (no prefix):   [MESH] Adjusted time ...     (clock derived from mesh time anchors)

Usage
-----
    uv run tools/scarfnet-log-viz.py logs/session.txt -o traces.json
    uv run tools/scarfnet-log-viz.py logs/2026-04-15-*.txt -o traces.json

    # Optional verbose lanes:
    uv run tools/scarfnet-log-viz.py session.txt --swarm --heartbeats -o traces.json

Viewing the output
------------------
Jaeger (simplest, runs locally):
    docker run -d --name jaeger -p 16686:16686 -p 4318:4318 jaegertracing/all-in-one
    curl -X POST http://localhost:4318/v1/traces \\
         -H 'Content-Type: application/json' --data @traces.json
    open http://localhost:16686

Grafana Tempo:
    Configure Tempo with OTLP receiver on :4318, then POST the JSON there.
    Visualise in Grafana with the Tempo datasource → "Search" tab.

Honeycomb / other cloud OTel backends:
    POST to your OTLP HTTP endpoint with the appropriate API key header.
"""

import sys
import re
import json
import hashlib
import argparse
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional, List, Dict, Tuple, Any
from datetime import datetime, timezone


# ---------------------------------------------------------------------------
# Regex patterns — every log line type we care about
# ---------------------------------------------------------------------------

# New-format timestamp prefix: [T+123456]
RE_TS = re.compile(r'^\[T\+(\d+)\]\s*')

# Mesh time anchor lines (present in old and new format — used to reconstruct
# relative timing when [T+N] is absent)
RE_MESH_TIME_ANCHOR = re.compile(r'\[MESH\] Adjusted time (\d+)ms\.')

# time_sync lane
RE_TIME_ADJUST = re.compile(r'\[MESH\] Adjusted time (\d+)ms\. Offset = (-?\d+)')

# topology lane
RE_NEW_NODE   = re.compile(r'\[MESH\]\[NEW node (\d+)\]')
RE_DROP_NODE  = re.compile(r'\[MESH\]\[DROP node (\d+)\]')
RE_CONN_LIST  = re.compile(r'Connection list \((\d+) nodes\): (.+)')
RE_TIMEOUT    = re.compile(r'CONNECTION: Time out reached')
RE_DELAY_MEAS = re.compile(r'\[MESH\] Delay to node (\d+) is (-?\d+) us')

# pattern lane
RE_PRESS      = re.compile(r'Event\.ePress to pattern (\S+) with randomizer (\d+) \(changeIndex: (\d+)\)')
RE_LONG_PRESS = re.compile(r'Event\.eLongPress to pattern (\S+) with randomizer (\d+) \(changeIndex: (\d+)\)')
RE_REMOTE_PAT = re.compile(r'Scarf::onReceivedData\(\)\. Changing pattern to (\S+) \(randomizer (-?\d+)\)')

# crash lane
RE_GURU      = re.compile(r'Guru Meditation Error')
RE_PANIC     = re.compile(r"Core (\d+) panic'ed \((\w+)\)")
RE_EXCVADDR  = re.compile(r'EXCVADDR: (0x[0-9a-fA-F]+)')
RE_BACKTRACE = re.compile(r'Backtrace:(.+)')

# memory lane
RE_MEM = re.compile(r'\[MEM\]\s+free[:\s=]+(\d+)\s+min-free[:\s=]+(\d+)', re.IGNORECASE)

# swarm lane
RE_SWARM_FIRST = re.compile(r'\[SWARM\] node (\d+) first delta: (-?\d+)ms')
RE_SWARM_DELTA = re.compile(r'\[SWARM\] node (\d+) delta: raw=(-?\d+)ms smoothed=(-?\d+)ms')

# heartbeat lane (optional — very noisy)
RE_SND = re.compile(r'\[SND\] (.+)')

# Heuristic: lines that belong to a crash dump rather than application logs
RE_CRASH_CONTINUATION = re.compile(
    r'^(0x[0-9a-fA-F]+|'
    r'ELF file SHA256:|'
    r'Rebooting\.\.\.|'
    r'ets [a-zA-Z]|'
    r'rst:|'
    r'configsip:|'
    r'mode:)'
)


# ---------------------------------------------------------------------------
# Event dataclass
# ---------------------------------------------------------------------------

@dataclass
class Event:
    time_ms: int                          # ms since session start (T+ or interpolated)
    lane: str                             # crash | topology | time_sync | pattern | memory | swarm | heartbeat
    name: str                             # span name shown in the viewer
    attrs: Dict[str, Any] = field(default_factory=dict)
    is_error: bool = False
    raw_line: str = ""


# ---------------------------------------------------------------------------
# Log parser
# ---------------------------------------------------------------------------

LANE_ORDER = ['crash', 'topology', 'time_sync', 'pattern', 'memory', 'swarm', 'heartbeat']


class LogParser:
    """Parse a single Scarfnet log file into a list of Events."""

    MAX_CRASH_LINES = 25  # stop accumulating crash context after this many lines

    def __init__(self, path: Path, include_heartbeats: bool = False, include_swarm: bool = False):
        self.path = path
        self.include_heartbeats = include_heartbeats
        self.include_swarm = include_swarm

    def parse(self) -> Tuple[List[Event], int]:
        """Return (events, session_duration_ms).

        Duration is computed from the first to the last event timestamp.
        """
        text = self.path.read_text(errors='replace')
        lines = text.splitlines()

        # Detect whether the file uses new [T+N] timestamps
        has_timestamps = any(RE_TS.match(ln) for ln in lines[:200])

        # First pass: build mesh-time anchor table for old-format logs.
        # anchor_table: [(line_index, mesh_time_ms)]
        anchor_table: List[Tuple[int, int]] = []
        if not has_timestamps:
            for i, line in enumerate(lines):
                m = RE_MESH_TIME_ANCHOR.search(line)
                if m:
                    anchor_table.append((i, int(m.group(1))))

        events: List[Event] = []
        crash_buf: List[str] = []
        crash_time_ms: int = 0

        for i, raw in enumerate(lines):
            raw = raw.rstrip()
            if not raw:
                continue

            time_ms = self._get_time_ms(raw, i, anchor_table) or 0

            # ── crash block accumulation ──────────────────────────────────
            if crash_buf:
                crash_buf.append(raw)
                should_close = (
                    RE_BACKTRACE.search(raw)
                    or len(crash_buf) >= self.MAX_CRASH_LINES
                    # Normal log line appearing after crash dump closes the block
                    or (raw.startswith('[') and not RE_CRASH_CONTINUATION.match(raw))
                )
                if should_close:
                    events.append(self._make_crash_event(crash_buf, crash_time_ms))
                    crash_buf = []
                continue

            if RE_GURU.search(raw) or RE_PANIC.search(raw):
                crash_buf = [raw]
                crash_time_ms = time_ms
                continue

            ev = self._parse_event(raw, time_ms)
            if ev is not None:
                events.append(ev)

        # Flush any open crash block (crash at end of file with no backtrace)
        if crash_buf:
            events.append(self._make_crash_event(crash_buf, crash_time_ms))

        events.sort(key=lambda e: e.time_ms)

        # Normalize timestamps to session-relative (first event = T+0)
        if events:
            t0 = events[0].time_ms
            for ev in events:
                ev.time_ms -= t0

        duration = (events[-1].time_ms - events[0].time_ms) if len(events) > 1 else 0
        return events, duration

    # ── timestamp extraction ───────────────────────────────────────────────

    def _get_time_ms(
        self,
        line: str,
        line_idx: int,
        anchor_table: List[Tuple[int, int]],
    ) -> Optional[int]:
        # New-format: [T+N]
        m = RE_TS.match(line)
        if m:
            return int(m.group(1))

        # This line itself is a mesh-time anchor
        m = RE_MESH_TIME_ANCHOR.search(line)
        if m:
            return int(m.group(1))

        # Interpolate from surrounding anchors
        if anchor_table:
            return _interpolate(line_idx, anchor_table)

        return None

    # ── per-line event parsing ─────────────────────────────────────────────

    def _parse_event(self, line: str, time_ms: int) -> Optional[Event]:
        s = RE_TS.sub('', line)  # strip timestamp prefix for matching

        # time_sync
        m = RE_TIME_ADJUST.search(s)
        if m:
            mesh_t, offset = int(m.group(1)), int(m.group(2))
            sign = '+' if offset >= 0 else ''
            big = abs(offset) > 500_000
            return Event(
                time_ms=time_ms,
                lane='time_sync',
                name=f'Clock {sign}{offset // 1000}ms',
                attrs={'offset_us': offset, 'mesh_time_ms': mesh_t},
                is_error=big,
                raw_line=line,
            )

        # topology — new node
        m = RE_NEW_NODE.search(s)
        if m:
            return Event(time_ms=time_ms, lane='topology',
                         name=f'Node joined {m.group(1)}',
                         attrs={'node_id': m.group(1)}, raw_line=line)

        # topology — dropped node
        m = RE_DROP_NODE.search(s)
        if m:
            return Event(time_ms=time_ms, lane='topology',
                         name=f'Node dropped {m.group(1)}',
                         attrs={'node_id': m.group(1)}, raw_line=line)

        # topology — timeout
        if RE_TIMEOUT.search(s):
            return Event(time_ms=time_ms, lane='topology',
                         name='Connection timeout', attrs={}, raw_line=line)

        # topology — connection list snapshot
        m = RE_CONN_LIST.search(s)
        if m:
            count, nodes_str = int(m.group(1)), m.group(2).strip()
            return Event(
                time_ms=time_ms,
                lane='topology',
                name=f'Topology: {count} nodes',
                attrs={'node_count': count, 'nodes': nodes_str[:300]},
                raw_line=line,
            )

        # topology — delay measurement result
        m = RE_DELAY_MEAS.search(s)
        if m:
            node_id, delay_us = m.group(1), int(m.group(2))
            return Event(
                time_ms=time_ms,
                lane='topology',
                name=f'Delay→{node_id}: {delay_us // 1000}ms',
                attrs={'node_id': node_id, 'delay_us': delay_us},
                is_error=delay_us < 0,
                raw_line=line,
            )

        # pattern — local short press
        m = RE_PRESS.search(s)
        if m:
            pat, rnd, ci = m.group(1), int(m.group(2)), int(m.group(3))
            return Event(
                time_ms=time_ms, lane='pattern',
                name=f'LOCAL → {pat}',
                attrs={'pattern': pat, 'randomizer': rnd, 'change_index': ci},
                raw_line=line,
            )

        # pattern — local long press
        m = RE_LONG_PRESS.search(s)
        if m:
            pat, rnd, ci = m.group(1), int(m.group(2)), int(m.group(3))
            return Event(
                time_ms=time_ms, lane='pattern',
                name=f'LOCAL re-rand → {pat}',
                attrs={'pattern': pat, 'randomizer': rnd, 'change_index': ci},
                raw_line=line,
            )

        # pattern — remote pattern accepted
        m = RE_REMOTE_PAT.search(s)
        if m:
            pat, rnd = m.group(1), int(m.group(2))
            return Event(
                time_ms=time_ms, lane='pattern',
                name=f'REMOTE → {pat}',
                attrs={'pattern': pat, 'randomizer': rnd},
                raw_line=line,
            )

        # memory
        m = RE_MEM.search(s)
        if m:
            free, min_free = int(m.group(1)), int(m.group(2))
            return Event(
                time_ms=time_ms,
                lane='memory',
                name=f'Heap {free // 1024}KB free ({min_free // 1024}KB min)',
                attrs={'free_bytes': free, 'min_free_bytes': min_free},
                is_error=free < 40_000,
                raw_line=line,
            )

        # swarm (optional)
        if self.include_swarm:
            m = RE_SWARM_FIRST.search(s)
            if m:
                node_id, delta = m.group(1), int(m.group(2))
                return Event(
                    time_ms=time_ms, lane='swarm',
                    name=f'SWARM first {node_id}: Δ={delta}ms',
                    attrs={'node_id': node_id, 'raw_delta_ms': delta},
                    is_error=abs(delta) > 60_000,
                    raw_line=line,
                )

            m = RE_SWARM_DELTA.search(s)
            if m:
                node_id, raw, smooth = m.group(1), int(m.group(2)), int(m.group(3))
                return Event(
                    time_ms=time_ms, lane='swarm',
                    name=f'SWARM {node_id}: raw={raw}ms sm={smooth}ms',
                    attrs={'node_id': node_id, 'raw_delta_ms': raw, 'smoothed_ms': smooth},
                    is_error=abs(smooth) > 60_000,
                    raw_line=line,
                )

        # heartbeat (optional)
        if self.include_heartbeats:
            m = RE_SND.search(s)
            if m:
                return Event(
                    time_ms=time_ms, lane='heartbeat',
                    name='SND heartbeat',
                    attrs={'json': m.group(1)[:120]},
                    raw_line=line,
                )

        return None

    @staticmethod
    def _make_crash_event(buf: List[str], time_ms: int) -> Event:
        excvaddr = next(
            (RE_EXCVADDR.search(ln).group(1) for ln in buf if RE_EXCVADDR.search(ln)),
            '',
        )
        backtrace = next(
            (RE_BACKTRACE.search(ln).group(1).strip() for ln in buf if RE_BACKTRACE.search(ln)),
            '',
        )
        headline = buf[0][:80]
        return Event(
            time_ms=time_ms,
            lane='crash',
            name=f'CRASH: {headline}',
            attrs={'excvaddr': excvaddr, 'backtrace': backtrace[:300]},
            is_error=True,
            raw_line=buf[0],
        )


# ---------------------------------------------------------------------------
# OTLP JSON helpers
# ---------------------------------------------------------------------------

def _span_id(seed: str) -> str:
    return hashlib.sha256(seed.encode()).digest()[:8].hex()


def _trace_id(seed: str) -> str:
    return hashlib.sha256(seed.encode()).digest()[:16].hex()


def _ns(ms: int) -> str:
    """Milliseconds → nanoseconds, encoded as string (OTLP JSON requirement)."""
    return str(ms * 1_000_000)


def _attr(key: str, value: Any) -> dict:
    if isinstance(value, bool):
        return {'key': key, 'value': {'boolValue': value}}
    if isinstance(value, int):
        return {'key': key, 'value': {'intValue': str(value)}}
    if isinstance(value, float):
        return {'key': key, 'value': {'doubleValue': value}}
    return {'key': key, 'value': {'stringValue': str(value)}}


def _interpolate(line_idx: int, anchors: List[Tuple[int, int]]) -> int:
    """Linear interpolation / extrapolation between mesh-time anchor points."""
    before = [(i, t) for i, t in anchors if i <= line_idx]
    after  = [(i, t) for i, t in anchors if i >  line_idx]
    if not before:
        return anchors[0][1]
    if not after:
        return before[-1][1]
    i0, t0 = before[-1]
    i1, t1 = after[0]
    if i1 == i0:
        return t0
    frac = (line_idx - i0) / (i1 - i0)
    return int(t0 + frac * (t1 - t0))


# ---------------------------------------------------------------------------
# OTLP trace builder
# ---------------------------------------------------------------------------

def build_otlp(
    file_records: List[Tuple[Path, List[Event], int]],
    base_date: datetime,
) -> dict:
    """Build a complete OTLP JSON document.

    Each (path, events, duration_ms) tuple becomes one resourceSpan
    (one 'service' in the viewer), all sharing the same traceId so
    multi-file sessions appear in a single trace.
    """
    base_ns_int = int(base_date.timestamp() * 1e9)

    # Shared traceId: deterministic from date + all filenames
    trace_seed = base_date.strftime('%Y-%m-%d') + '|'.join(p.stem for p, _, _ in file_records)
    tid = _trace_id(trace_seed)

    resource_spans = []

    for file_path, events, duration_ms in file_records:
        if not events:
            continue

        service_name = f'scarfnet.{file_path.stem}'
        # Use a deterministic but file-unique counter seed
        id_seed_base = f'{file_path.stem}:'
        counter = [0]

        def sid(tag: str = '') -> str:
            counter[0] += 1
            return _span_id(f'{id_seed_base}{counter[0]}:{tag}')

        session_start_ns = base_ns_int
        session_end_ns   = base_ns_int + max(duration_ms, 1) * 1_000_000

        # Root span — full session
        root_id = sid('root')
        spans = [{
            'traceId': tid,
            'spanId': root_id,
            'name': f'session: {file_path.name}',
            'kind': 1,
            'startTimeUnixNano': str(session_start_ns),
            'endTimeUnixNano':   str(session_end_ns),
            'attributes': [
                _attr('scarfnet.log_file', file_path.name),
                _attr('scarfnet.event_count', len(events)),
                _attr('scarfnet.duration_ms', duration_ms),
            ],
            'status': {'code': 1},
        }]

        # Lane container spans — one per lane type present, full session duration.
        # These give you collapsible swim-lane rows in the trace viewer.
        lanes_present = sorted(
            {ev.lane for ev in events},
            key=lambda l: LANE_ORDER.index(l) if l in LANE_ORDER else 99,
        )
        lane_ids: Dict[str, str] = {}
        for lane in lanes_present:
            lid = sid(f'lane-{lane}')
            lane_ids[lane] = lid
            lane_count = sum(1 for ev in events if ev.lane == lane)
            spans.append({
                'traceId': tid,
                'spanId': lid,
                'parentSpanId': root_id,
                'name': f'[{lane}]',
                'kind': 1,
                'startTimeUnixNano': str(session_start_ns),
                'endTimeUnixNano':   str(session_end_ns),
                'attributes': [
                    _attr('scarfnet.lane', lane),
                    _attr('scarfnet.event_count', lane_count),
                ],
                'status': {'code': 1},
            })

        # Event spans — one per event, 1 ms duration, child of their lane.
        for ev in events:
            ev_start = base_ns_int + ev.time_ms * 1_000_000
            ev_end   = ev_start + 1_000_000  # 1 ms — point-in-time marker
            parent   = lane_ids.get(ev.lane, root_id)

            spans.append({
                'traceId': tid,
                'spanId': sid(f'{ev.lane}-{ev.time_ms}'),
                'parentSpanId': parent,
                'name': ev.name,
                'kind': 1,
                'startTimeUnixNano': str(ev_start),
                'endTimeUnixNano':   str(ev_end),
                'attributes': [_attr(k, v) for k, v in ev.attrs.items() if v != ''],
                'status': {'code': 2 if ev.is_error else 1},
            })

        resource_spans.append({
            'resource': {
                'attributes': [
                    _attr('service.name', service_name),
                    _attr('scarfnet.log_file', file_path.name),
                ],
            },
            'scopeSpans': [{
                'scope': {'name': 'scarfnet-log-parser', 'version': '1.0'},
                'spans': spans,
            }],
        })

    return {'resourceSpans': resource_spans}


# ---------------------------------------------------------------------------
# Date extraction from filename
# ---------------------------------------------------------------------------

_RE_DATE = re.compile(r'(\d{4})-(\d{2})-(\d{2})')


def date_from_path(path: Path) -> Optional[datetime]:
    m = _RE_DATE.search(path.name)
    if m:
        try:
            return datetime(int(m.group(1)), int(m.group(2)), int(m.group(3)),
                            tzinfo=timezone.utc)
        except ValueError:
            pass
    return None


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(
        description='Parse Scarfnet log files → OTLP JSON traces',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument('logs', nargs='+', type=Path,
                    help='Log file(s) to parse (glob-expanded by shell)')
    ap.add_argument('-o', '--output', type=Path, default=None,
                    help='Output file (default: stdout)')
    ap.add_argument('--swarm', action='store_true',
                    help='Include [SWARM] arrival delta events (adds ~1 span per heartbeat per peer)')
    ap.add_argument('--heartbeats', action='store_true',
                    help='Include sent heartbeat spans (very verbose, ~1 span per 5s)')
    args = ap.parse_args()

    file_records: List[Tuple[Path, List[Event], int]] = []
    for log_path in args.logs:
        if not log_path.exists():
            print(f'Error: {log_path} not found', file=sys.stderr)
            sys.exit(1)
        parser = LogParser(log_path,
                           include_heartbeats=args.heartbeats,
                           include_swarm=args.swarm)
        events, duration = parser.parse()
        lanes = {}
        for ev in events:
            lanes[ev.lane] = lanes.get(ev.lane, 0) + 1
        lane_summary = ', '.join(f'{l}={c}' for l, c in sorted(lanes.items()))
        print(f'{log_path.name}: {len(events)} events over {duration}ms  [{lane_summary}]',
              file=sys.stderr)
        file_records.append((log_path, events, duration))

    # Base time: date from first filename, file mtime, or today
    base_date = date_from_path(args.logs[0])
    if base_date is None:
        mtime = args.logs[0].stat().st_mtime
        base_date = datetime.fromtimestamp(mtime, tz=timezone.utc).replace(
            hour=0, minute=0, second=0, microsecond=0)
    print(f'Session base: {base_date.date()} UTC', file=sys.stderr)

    otlp_doc = build_otlp(file_records, base_date)

    payload = json.dumps(otlp_doc, indent=2)
    if args.output:
        args.output.write_text(payload)
        print(f'Written {args.output} ({len(payload) // 1024}KB)', file=sys.stderr)
    else:
        print(payload)


if __name__ == '__main__':
    main()
