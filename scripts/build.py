#!/usr/bin/env python3
"""Build the C++17 programs using only the Python standard library."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
TARGETS = {
    "thor_basic": (["src/basic/main.cpp"], ["-DTHOR_USE_COALESCING=0"]),
    "thor_np": (["src/basic/main.cpp"], ["-DTHOR_USE_COALESCING=1"]),
    "thor": (["src/thor/main.cpp"], []),
    "stream": (["src/stream/main.cpp"], []),
    "stream_profile": (["src/stream/profile.cpp"], []),
    "recurs_official": (
        [
            "src/recurs/main.cpp",
            "src/vendor/TemporalHIndex/thix_recursive.cpp",
            "src/vendor/TemporalGraph/TemporalGraphs.cpp",
        ],
        ["-include", "src/vendor/compat.hpp"],
    ),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--jobs", "-j", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--targets", nargs="+", choices=TARGETS, default=list(TARGETS))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    compiler = shlex.split(args.cxx)
    if not compiler:
        parser.error("--cxx must specify a compiler")
    flags = shlex.split(os.environ.get("CXXFLAGS", "-O3 -DNDEBUG -std=c++17"))
    output_dir = args.build_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    targets = list(dict.fromkeys(args.targets))

    def compile_target(name, staging_dir):
        sources, extra_flags = TARGETS[name]
        output = staging_dir / name
        command = [*compiler, *flags, *extra_flags, *sources, "-o", str(output)]
        result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(
                f"{name}: compiler exited with status {result.returncode}\n"
                f"{result.stdout}{result.stderr}"
            )
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        output.replace(output_dir / name)
        return name

    print("Compiler:", shlex.join(compiler), flush=True)
    print("Flags:", shlex.join(flags), flush=True)
    with tempfile.TemporaryDirectory(prefix=".compile-", dir=output_dir) as temporary:
        staging_dir = Path(temporary)
        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            jobs = {
                executor.submit(compile_target, name, staging_dir): name
                for name in targets
            }
            failed = False
            for future in as_completed(jobs):
                try:
                    print("Built", future.result(), flush=True)
                except (OSError, RuntimeError) as error:
                    failed = True
                    print(error, file=sys.stderr)
            if failed:
                return 1
    print("Executables:", output_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
