#!/usr/bin/env python3
"""Benchmark ONNX Runtime inference after concatenating several events.

Each input archive represents one fully preprocessed event and must contain
float32 arrays named inp1, inp2, inp3 and inp4. Their first dimension is the
number of cluster groups for that event, matching MLPFClusterProducer's inputs.

Example:
  python3 scripts/benchmark_event_batches.py data/clus_tex.onnx prepared/*.npz \
      --batch-events 1 2 4 8 --repetitions 20
"""
import argparse
import json
import time
from pathlib import Path

import numpy as np

INPUTS = ("inp1", "inp2", "inp3", "inp4")


def load_event(path):
    with np.load(path) as archive:
        missing = set(INPUTS) - set(archive.files)
        if missing:
            raise ValueError(f"{path}: missing {sorted(missing)}")
        event = {name: np.ascontiguousarray(archive[name], dtype=np.float32) for name in INPUTS}
    n_groups = event[INPUTS[0]].shape[0]
    if n_groups == 0 or any(event[name].shape[0] != n_groups for name in INPUTS):
        raise ValueError(f"{path}: inputs must have the same non-zero leading dimension")
    return event


def chunks(events, size):
    for begin in range(0, len(events) - size + 1, size):
        yield events[begin : begin + size]


def one_measurement(session, event_chunk):
    start = time.perf_counter()
    inputs = {name: np.concatenate([event[name] for event in event_chunk], axis=0) for name in INPUTS}
    packed = time.perf_counter()
    session.run(None, inputs)  # synchronous: includes copies and output retrieval
    done = time.perf_counter()
    return packed - start, done - packed, done - start, inputs["inp1"].shape[0]


def percentile(values, p):
    return float(np.percentile(values, p)) if values else 0.0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("model", type=Path)
    parser.add_argument("events", type=Path, nargs="*", help="one .npz archive per event")
    parser.add_argument("--batch-events", type=int, nargs="+", default=[1, 2, 4, 8])
    parser.add_argument("--warmup", type=int, default=5)
    parser.add_argument("--repetitions", type=int, default=20)
    parser.add_argument("--provider", choices=("cuda", "cpu"), default="cuda")
    parser.add_argument("--synthetic-events", type=int,
                        help="generate this many representative events instead of reading .npz archives")
    parser.add_argument("--groups-per-event", type=int, default=8,
                        help="groups in each synthetic event (default: 8)")
    parser.add_argument("--json", type=Path, help="optional machine-readable results file")
    args = parser.parse_args()

    try:
        import onnxruntime as ort
    except ImportError as error:
        parser.error("onnxruntime is required; install the CUDA-enabled package in the CMSSW runtime")

    providers = ["CUDAExecutionProvider", "CPUExecutionProvider"] if args.provider == "cuda" else ["CPUExecutionProvider"]
    session = ort.InferenceSession(args.model, providers=providers)
    if args.provider == "cuda" and session.get_providers()[0] != "CUDAExecutionProvider":
        raise RuntimeError(f"CUDA provider unavailable; active providers: {session.get_providers()}")
    if args.synthetic_events is not None:
        if args.events:
            parser.error("event archives and --synthetic-events are mutually exclusive")
        if args.synthetic_events < 1 or args.groups_per_event < 1:
            parser.error("synthetic event and group counts must be positive")
        rng = np.random.default_rng(12345)
        events = [{
            "inp1": rng.random((args.groups_per_event, 20, 7, 7), dtype=np.float32),
            "inp2": rng.random((args.groups_per_event, 20, 2), dtype=np.float32),
            "inp3": rng.random((args.groups_per_event, 20), dtype=np.float32),
            "inp4": rng.random((args.groups_per_event, 20, 7, 7), dtype=np.float32),
        } for _ in range(args.synthetic_events)]
    else:
        if not args.events:
            parser.error("provide event archives or --synthetic-events")
        events = [load_event(path) for path in args.events]
    results = []

    for batch_size in args.batch_events:
        work = list(chunks(events, batch_size))
        if not work:
            print(f"batch_events={batch_size}: skipped (need at least {batch_size} event archives)")
            continue
        for _ in range(args.warmup):
            for event_chunk in work:
                one_measurement(session, event_chunk)
        timings = [one_measurement(session, event_chunk) for _ in range(args.repetitions) for event_chunk in work]
        pack, infer, total, groups = map(list, zip(*timings))
        elapsed = sum(total)
        result = {
            "batch_events": batch_size,
            "measurements": len(timings),
            "mean_groups": float(np.mean(groups)),
            "pack_ms_mean": 1e3 * float(np.mean(pack)),
            "inference_ms_mean": 1e3 * float(np.mean(infer)),
            "inference_ms_p95": 1e3 * percentile(infer, 95),
            "total_ms_mean": 1e3 * float(np.mean(total)),
            "events_per_second": batch_size * len(timings) / elapsed,
            "groups_per_second": sum(groups) / elapsed,
        }
        results.append(result)
        print("batch_events={batch_events:2d} mean_groups={mean_groups:6.1f} "
              "pack={pack_ms_mean:7.3f} ms infer={inference_ms_mean:7.3f} ms "
              "p95={inference_ms_p95:7.3f} ms total={total_ms_mean:7.3f} ms "
              "events/s={events_per_second:8.1f} groups/s={groups_per_second:9.1f}".format(**result))
    if args.json:
        args.json.write_text(json.dumps({"provider": session.get_providers(), "results": results}, indent=2) + "\n")


if __name__ == "__main__":
    main()
