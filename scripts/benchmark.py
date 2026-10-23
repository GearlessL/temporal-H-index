#!/usr/bin/env python3
"""Run serial temporal H-index benchmarks and summarize successful runs."""

import argparse
import csv
import json
import math
import os
from pathlib import Path
import signal
import statistics
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
BINARIES = {
    "THOR-Basic": "thor_basic",
    "THOR-NP": "thor_np",
    "THOR": "thor",
    "STREAM": "stream",
    "Recurs-official": "recurs_official",
}
METHODS = list(BINARIES)
RUN_FIELDS = [
    "run_id", "dataset", "graph", "order", "method", "repeat", "status",
    "returncode", "load_s", "prep_s", "solve_s", "algorithm_s", "total_s",
    "wall_s", "peak_rss_mib", "vertices", "edges", "run_directory", "error",
]
SUMMARY_FIELDS = [
    "dataset", "graph", "order", "method", "requested_repeats",
    "successful_repeats", "status", "statuses", "algorithm_median_s",
    "algorithm_min_s", "algorithm_max_s", "load_median_s", "prep_median_s",
    "solve_median_s", "total_median_s", "wall_median_s", "peak_rss_median_mib",
]


def command(build, method, graph, order, metrics):
    args = [str(build / BINARIES[method]), str(graph), str(order)]
    return args + ["--metrics", str(metrics)]


def run_process(args, stdout, stderr, timeout):
    """Collect peak RSS for this process, including on timeout or interruption."""
    start = time.monotonic()
    try:
        proc = subprocess.Popen(
            args, stdout=stdout, stderr=stderr, start_new_session=True,
            env={**os.environ, "OMP_NUM_THREADS": "1", "OPENBLAS_NUM_THREADS": "1",
                 "MKL_NUM_THREADS": "1"},
        )
    except OSError as exc:
        return {"status": "failed", "wall_s": time.monotonic() - start,
                "error": str(exc)}
    status = "ok"
    try:
        while True:
            pid, wait_status, usage = os.wait4(proc.pid, os.WNOHANG)
            if pid:
                break
            remaining = timeout - (time.monotonic() - start)
            if remaining <= 0:
                status = "timeout"
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                _, wait_status, usage = os.wait4(proc.pid, 0)
                break
            time.sleep(min(0.05, remaining))
    except KeyboardInterrupt:
        status = "interrupted"
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        _, wait_status, usage = os.wait4(proc.pid, 0)
    returncode = os.waitstatus_to_exitcode(wait_status)
    proc.returncode = returncode
    if status == "ok" and returncode != 0:
        status = "unsupported" if returncode == 3 else "failed"
    rss_bytes = usage.ru_maxrss * (1 if sys.platform == "darwin" else 1024)
    return {"status": status, "returncode": returncode,
            "wall_s": time.monotonic() - start, "peak_rss_mib": rss_bytes / 1024**2}


def write_summary(path, records, graphs, orders, methods, repeats):
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=SUMMARY_FIELDS)
        writer.writeheader()
        for graph in graphs:
            for order in orders:
                for method in methods:
                    rows = [r for r in records if r["graph"] == str(graph)
                            and r["order"] == order and r["method"] == method]
                    good = [r for r in rows if r["status"] == "ok"]
                    statuses = sorted({r["status"] for r in rows})
                    if len(rows) < repeats:
                        statuses.append("not_run")
                    row = {
                        "dataset": graph.stem, "graph": str(graph), "order": order,
                        "method": method, "requested_repeats": repeats,
                        "successful_repeats": len(good),
                        "status": "complete" if len(good) == repeats else
                                  "partial" if good else "no_success",
                        "statuses": ";".join(statuses),
                    }
                    if good:
                        values = [r["algorithm_s"] for r in good]
                        row.update(algorithm_median_s=statistics.median(values),
                                   algorithm_min_s=min(values), algorithm_max_s=max(values))
                        for key in ("load", "prep", "solve", "total", "wall"):
                            row[key + "_median_s"] = statistics.median(
                                r[key + "_s"] for r in good)
                        row["peak_rss_median_mib"] = statistics.median(
                            r["peak_rss_mib"] for r in good)
                    writer.writerow(row)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("graphs", nargs="+", type=Path, metavar="GRAPH")
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--orders", nargs="+", type=int, default=[32, 128])
    parser.add_argument("--methods", nargs="+", choices=METHODS, default=["THOR"])
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--output", type=Path, default=ROOT / "results" / "benchmark")
    parser.add_argument("--timeout-s", type=float, default=3600,
                        help="Per-process wall-time limit, including loading and output")
    parser.add_argument("--keep-answers", action="store_true",
                        help="Save per-vertex output as answers.txt for each run")
    args = parser.parse_args()
    if not hasattr(os, "wait4"):
        parser.error("benchmark.py requires a POSIX system with os.wait4 (e.g. Linux)")
    if args.repeats < 1 or any(n < 0 or n >= 2**31 - 1 for n in args.orders):
        parser.error("repeats must be positive; orders must be in [0, INT_MAX-1]")
    if not math.isfinite(args.timeout_s) or args.timeout_s <= 0:
        parser.error("--timeout-s must be finite and positive")
    args.graphs = [p.resolve() for p in args.graphs]
    args.build = args.build.resolve()
    args.output = args.output.resolve()
    for values, label in ((args.graphs, "graphs"), (args.orders, "orders"),
                          (args.methods, "methods")):
        if len(set(values)) != len(values):
            parser.error("duplicate " + label)
    for graph in args.graphs:
        if not graph.is_file():
            parser.error("input file not found: " + str(graph))
    for method in args.methods:
        binary = args.build / BINARIES[method]
        if not binary.is_file() or not os.access(binary, os.X_OK):
            parser.error("executable not found: " + str(binary) + "; build the project first")
    if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
        parser.error("--output must be a new or empty directory to preserve existing files")
    args.output.mkdir(parents=True, exist_ok=True)
    records = []
    total = len(args.graphs) * len(args.orders) * len(args.methods) * args.repeats
    interrupted = False

    def stop(_signum, _frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, stop)
    with (args.output / "runs.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=RUN_FIELDS)
        writer.writeheader()
        for graph in args.graphs:
            for order in args.orders:
                for repeat in range(1, args.repeats + 1):
                    for method in args.methods:
                        run_id = f"{len(records) + 1:06d}"
                        run_dir = args.output / "runs" / run_id
                        run_dir.mkdir(parents=True)
                        metrics_path = run_dir / "metrics.json"
                        row = {"run_id": run_id, "dataset": graph.stem, "graph": str(graph),
                               "order": order, "method": method, "repeat": repeat,
                               "run_directory": str(run_dir.relative_to(args.output))}
                        print(f"[{len(records) + 1}/{total}] {graph.name} order={order} "
                              f"{method} repeat={repeat}", flush=True)
                        answer_path = run_dir / "answers.txt" if args.keep_answers else os.devnull
                        with open(answer_path, "wb") as stdout, (run_dir / "stderr.txt").open("wb") as stderr:
                            row.update(run_process(command(args.build, method, graph, order,
                                                           metrics_path), stdout, stderr, args.timeout_s))
                        if row["status"] == "ok":
                            try:
                                metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
                                row.update({key: float(metrics[key])
                                            for key in ("load_s", "prep_s", "solve_s")})
                                row["algorithm_s"] = row["prep_s"] + row["solve_s"]
                                row["total_s"] = row["load_s"] + row["algorithm_s"]
                                row.update({key: metrics.get(key, "") for key in ("vertices", "edges")})
                            except (OSError, ValueError, KeyError, TypeError) as exc:
                                row.update(status="failed", error="cannot read run metrics: " + str(exc))
                        writer.writerow(row)
                        handle.flush()
                        records.append(row)
                        print("  " + row["status"], flush=True)
                        if row["status"] == "interrupted":
                            interrupted = True
                            break
                    if interrupted:
                        break
                if interrupted:
                    break
            if interrupted:
                break
    write_summary(args.output / "summary.csv", records, args.graphs,
                  args.orders, args.methods, args.repeats)
    print("Results:", args.output)
    if interrupted:
        return 130
    return 0 if all(r["status"] == "ok" for r in records) else 1


if __name__ == "__main__":
    sys.exit(main())
