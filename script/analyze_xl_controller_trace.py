#!/usr/bin/env python3
"""Analyze private XL hardware chronology without changing reference clocks."""
import argparse
import collections
import csv
import json
import statistics
from pathlib import Path


def read(path):
    with path.open() as stream:
        return list(csv.DictReader(stream))


def spread(values):
    return dict(count=len(values), minimum=min(values), median=statistics.median(values), maximum=max(values)) if values else None


def analyze(directory):
    metadata = read(directory / 'metadata.csv')
    assert len(metadata) == 1 and metadata[0]['traced'] == '1', 'Expected one traced fixture'
    meta = metadata[0]
    events = read(directory / 'events.csv')
    for event in events:
        for key in event:
            if key != 'kind':
                event[key] = int(event[key])
    # Some CPU hooks report a previously scheduled bus event. Physical ticks,
    # rather than callback/file order, establish the chronology.
    events.sort(key=lambda event: event['tick'])
    kinds = collections.Counter(event['kind'] for event in events)
    irq_start = None
    irq_spans = []
    for event in events:
        if event['kind'] == 'irq_entry':
            assert irq_start is None, 'Nested IRQ in trace'
            irq_start = event['cpu_state']
        elif event['kind'] == 'irq_return':
            if irq_start is not None:
                assert event['cpu_state'] - irq_start == event['aux'], 'IRQ acknowledgement accounting differs'
                irq_spans.append((irq_start, event['cpu_state']))
                irq_start = None
    controllers = []
    for name, pc, column in [('mod', 0xad5c, 'mod_calls'), ('slow', 0x82cf, 'slow_calls'), ('fast', 0x81b6, 'fast_calls')]:
        entries = [event for event in events if event['kind'] == 'controller_entry' and event['pc'] == pc]
        returns = [event for event in events if event['kind'] == 'controller_return' and event['value'] == pc]
        assert len(entries) == int(meta[column]), 'Controller entry count differs from metadata'
        gaps = [right['cpu_state'] - left['cpu_state'] for left, right in zip(entries, entries[1:])]
        local_durations = []
        for event in returns:
            end = event['cpu_state']; begin = end - event['aux']
            irq_states = sum(max(0, min(end, right) - max(begin, left)) for left, right in irq_spans)
            local_durations.append(event['aux'] - irq_states)
        controllers.append(dict(name=name, pc=pc, entries=len(entries), returns=len(returns),
            entry_rate_hz=len(entries) / float(meta['seconds']),
            callers=dict(collections.Counter(entry['value'] for entry in entries)),
            gaps_cpu_states=spread(gaps), gap_histogram=dict(collections.Counter(gaps)),
            duration_cpu_states=spread([event['aux'] for event in returns]),
            duration_histogram=dict(collections.Counter(event['aux'] for event in returns)),
            duration_excluding_irq_states=spread(local_durations),
            duration_excluding_irq_histogram=dict(collections.Counter(local_durations))))
    for kind, column in [('wcs_grant', 'grants'), ('wcs_commit', 'commits'), ('fetch_displaced', 'displaced'), ('irq_entry', 'irqs')]:
        assert kinds[kind] == int(meta[column]), 'Hardware event count differs from metadata'
    writes = [event for event in events if event['kind'] == 'write_bus_t1']
    port_samples = [event for event in events if event['kind'] == 'port_read_sample']
    sample_offsets = collections.defaultdict(list)
    if port_samples:
        instructions = collections.Counter((event['cpu_state'], event['pc'], event['lane'], event['value'])
            for event in events if event['kind'] == 'port_read')
        samples = collections.Counter((event['aux'], event['pc'], event['lane'], event['value']) for event in port_samples)
        assert samples == instructions, 'Physical port sample/instruction records differ'
        for event in port_samples:
            offset = event['tick'] - event['aux'] * 281250
            assert offset >= 0, 'Port sample precedes its instruction'
            sample_offsets[event['lane']].append(offset)
    commits = {(event['tick'], event['row'], event['lane'], event['value']) for event in events if event['kind'] == 'wcs_commit'}
    paired = sum((event['aux'], event['row'], event['lane'], event['value']) in commits for event in writes)
    pending = collections.deque()
    delays, write_commit_delays = [], []
    unmatched_grants = 0
    grants = {}
    for event in events:
        if event['kind'] in ('wcs_write_request', 'wcs_read_request'):
            pending.append(event)
        elif event['kind'] == 'wcs_grant':
            if pending:
                request = pending.popleft()
                delays.append(event['tick'] - request['tick'])
                grants[(request['row'], request['lane'])] = (event['tick'], request['kind'])
            else:
                unmatched_grants += 1  # observation starts with an already pending request
        elif event['kind'] == 'wcs_commit':
            grant = grants.pop((event['row'], event['lane']), None)
            if grant:
                assert grant[1] == 'wcs_write_request', 'Commit belongs to a read request'
                write_commit_delays.append(event['tick'] - grant[0])
    # Fixed independent T&C write propagation: capture 4.5 ns, one row,
    # three master slots, -12 ns select lead and +10 ns write propagation.
    assert all(delay == 226440 for delay in write_commit_delays), 'Unexpected WCS grant/commit propagation'
    result = dict(metadata=meta, event_counts=dict(kinds), controllers=controllers,
        grant_request_delay_ticks=spread(delays), grant_commit_delay_ticks=spread(write_commit_delays),
        paired_cpu_bus_commits=paired, cpu_bus_writes=len(writes),
        unpaired_cpu_bus_writes=len(writes) - paired, unpaired_trace_commits=len(commits) - paired,
        initial_unmatched_grants=unmatched_grants, terminal_ungranted_requests=len(pending),
        irq_duration_cpu_states=spread([right - left for left, right in irq_spans]),
        irq_duration_histogram=dict(collections.Counter(right - left for left, right in irq_spans)),
        port_reads=dict(collections.Counter(event['lane'] for event in events if event['kind'] == 'port_read')),
        port_sample_offset_ticks={port: spread(values) for port, values in sample_offsets.items()},
        interpretation='physical-key independent firmware trace; finite window endpoints retained; no native scheduler acceptance claim')
    (directory / 'controller-summary.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    directory = args.directory.resolve()
    assert root / 'build' in directory.parents, 'Private output must stay under ignored build/'
    result = analyze(directory)
    print(json.dumps(dict(rates_hz={entry['name']: entry['entry_rate_hz'] for entry in result['controllers']},
        counts={key: result['event_counts'].get(key, 0) for key in ('wcs_grant', 'wcs_commit', 'fetch_displaced', 'irq_entry')},
        paired_cpu_bus_commits=result['paired_cpu_bus_commits']), indent=2))


if __name__ == '__main__':
    main()
