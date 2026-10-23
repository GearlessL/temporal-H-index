#!/usr/bin/env python3
"""Replace edge transition times with deterministic experimental policies."""

import argparse
from collections import Counter
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import sys


POLICIES = ["U1", "U0", "U4", "M1", "M2", "H"]
TIME_MIN = -(2**63)
TIME_MAX = 2**63 - 1


def duration(policy, u, v, departure, seed, undirected):
    if policy in ("U1", "U0", "U4"):
        return int(policy[1:])
    if undirected:
        u, v = min(u, v), max(u, v)
    value = int.from_bytes(hashlib.blake2b(
        f"{seed}|{policy}|{u}|{v}|{departure}".encode(), digest_size=8
    ).digest(), "big")
    if policy == "M1":
        return [0, 1, 2, 4, 8][value % 5]
    if policy == "M2":
        return [1, 2, 4, 8, 16][value % 5]
    return 1 if value % 100 < 90 else 4 if value % 100 < 99 else 64


def signed_integer(value):
    result = int(value)
    if result < TIME_MIN or result > TIME_MAX:
        raise ValueError("integer is outside the signed 64-bit range")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("graph", type=Path, metavar="GRAPH")
    parser.add_argument("--output", type=Path, required=True,
                        help="New or empty directory for POLICY.tg files and summary.json")
    parser.add_argument("--policies", nargs="+", choices=POLICIES, default=POLICIES)
    parser.add_argument("--seed", type=int, default=2027)
    parser.add_argument("--undirected", action="store_true",
                        help="Assign the same duration to existing reciprocal events; does not add edges")
    args = parser.parse_args()
    args.graph = args.graph.resolve()
    args.output = args.output.resolve()
    if not args.graph.is_file():
        parser.error("input file not found: " + str(args.graph))
    if len(set(args.policies)) != len(args.policies):
        parser.error("duplicate policies")
    if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
        parser.error("--output must be a new or empty directory to preserve existing files")
    args.output.mkdir(parents=True, exist_ok=True)
    histograms = {policy: Counter() for policy in args.policies}
    edge_count = 0
    partials = {policy: args.output / (policy + ".tg.part") for policy in args.policies}
    try:
        with ExitStack() as stack:
            source = stack.enter_context(args.graph.open(encoding="utf-8"))
            handles = {policy: stack.enter_context(path.open("w", encoding="utf-8"))
                       for policy, path in partials.items()}
            for line_number, line in enumerate(source, 1):
                fields = line.split("#", 1)[0].split("%", 1)[0].split()
                try:
                    if not fields:
                        for handle in handles.values():
                            handle.write(line)
                        continue
                    if fields[0] == "@vertex":
                        if len(fields) != 2:
                            raise ValueError("expected @vertex id")
                        signed_integer(fields[1])
                        for handle in handles.values():
                            handle.write(line)
                        continue
                    if len(fields) not in (3, 4):
                        raise ValueError("expected u v departure [transition]")
                    u, v, departure = map(signed_integer, fields[:3])
                    if len(fields) == 4 and signed_integer(fields[3]) < 0:
                        raise ValueError("transition time must be nonnegative")
                    for policy, handle in handles.items():
                        transition = duration(policy, u, v, departure, args.seed, args.undirected)
                        if departure > TIME_MAX - transition:
                            raise ValueError("generated arrival time overflows signed 64-bit integer")
                        handle.write(f"{u} {v} {departure} {transition}\n")
                        histograms[policy][transition] += 1
                    edge_count += 1
                except ValueError as exc:
                    raise ValueError(f"{args.graph}:{line_number}: {exc}") from exc
        for policy, partial in partials.items():
            partial.replace(args.output / (policy + ".tg"))
        summary = {
            "source": str(args.graph), "edges": edge_count, "seed": args.seed,
            "undirected_pairing": args.undirected,
            "operation": "Replace transition times; preserve endpoints, departures, vertex declarations and multiplicity.",
            "policies": {policy: {"file": policy + ".tg",
                                  "duration_histogram": dict(sorted(histograms[policy].items()))}
                         for policy in args.policies},
        }
        (args.output / "summary.json").write_text(
            json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError) as exc:
        for partial in partials.values():
            partial.unlink(missing_ok=True)
        print(str(exc), file=sys.stderr)
        return 1
    print(f"Wrote {len(args.policies)} policies for {edge_count} edges to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
