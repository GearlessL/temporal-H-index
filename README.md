# THOR: Higher-Order Temporal H-Index Computation

C++ implementations of THOR, its basic and coalesced variants, and baseline
algorithms for computing the outward higher-order temporal H-index. The
repository includes timing scripts, workload profiling, and tools for
constructing transition-time variants.

For a temporal edge `(u, v, t, lambda)`, `t` is its departure time and `lambda`
is its nonnegative transition time. An edge can follow another edge when its
departure is at least the preceding edge's arrival time. At order zero, the
value of a state `(u, q)` is the number of outgoing edge events departing at or
after `q`. At each subsequent order, its value is the H-index of the
previous-order values of those edges' destination states `(v, t + lambda)`.
The programs report the requested-order value at `q = 0` for every vertex.

## Quick start

Requirements: a 64-bit Linux environment, a C++17 compiler, and Python 3.9 or
newer. The Python scripts use only the standard library.

From the repository root:

```bash
python3 scripts/build.py
./build/thor examples/example.tg 2
```

The example produces:

```text
0 1
1 1
2 0
3 0
4 0
```

The build creates executables in `build/`. To select a compiler or compiler
flags:

```bash
CXX=g++ CXXFLAGS="-O3 -DNDEBUG -std=c++17" python3 scripts/build.py
```

Use `--build-dir PATH` for a different output directory, `--jobs N` to control
parallel compilation, or `--targets thor stream` to build selected
executables. All programs and scripts provide `--help`.

To save the vertex scores and timing information:

```bash
mkdir -p results
./build/thor examples/example.tg 2 \
  --metrics results/example.json > results/example.txt
```

The example contains an isolated vertex and a repeated edge event. Real-world
datasets and previously measured results are not included.

## Algorithms

| Method | Executable | Description | Transition times |
| --- | --- | --- | --- |
| THOR-Basic | `thor_basic` | Layered computation over the relevant temporal states | Nonnegative integers |
| THOR-NP | `thor_np` | Layered computation with state coalescing | Nonnegative integers |
| THOR | `thor` | Coalesced computation with saturation and selective reuse | Nonnegative integers |
| STREAM | `stream` | Reimplementation of the STREAM algorithm from [Oettershagen et al.](#references-and-third-party-code) | Exactly `1` on every edge |
| Recurs-official | `recurs_official` | Upstream recursive implementation with a local input/output adapter | Nonnegative integers; nonnegative departures |

Every executable above uses the following interface:

```text
./build/EXECUTABLE GRAPH ORDER [--metrics METRICS_JSON]
```

For example:

```bash
./build/thor_basic examples/example.tg 2
./build/thor_np examples/example.tg 2
./build/stream examples/example.tg 2
./build/recurs_official examples/example.tg 2
```

All implementations are sequential. `ORDER` must be a nonnegative integer.
Order zero is supported.

## Input and output

Input is a whitespace-separated text file. Each edge occupies one line:

```text
source destination departure transition
```

The fourth column is optional and defaults to `1`. For example:

```text
# source destination departure transition
0 1 0 1
0 2 1 1
1 2 1 1
@vertex 3
```

`@vertex ID` declares a vertex explicitly, including an isolated vertex. Vertex
IDs may be arbitrary signed 64-bit integers and need not be contiguous. The
program assigns compact internal IDs and restores the original IDs in the
output.

- Departure times and transition times must be integers representable with
  signed 64-bit storage. Transition times must be nonnegative, and their sum
  with the departure time must not overflow.
- Lines may be unsorted. Blank lines and text following `#` or `%` are ignored.
- Each line represents a directed edge event. For an undirected input, include
  both directions explicitly.
- Duplicate events and self-loops are retained. Duplicate events contribute
  separately to the temporal degree and H-index recurrence.
- THOR, THOR-Basic, and THOR-NP accept negative departure times. The reported
  root query remains `q = 0`; events departing before zero are not eligible
  for this root query. STREAM also skips events departing before zero. The
  Recurs adapter rejects negative departures.
- The implementation uses compact 32-bit internal indices and rejects graphs
  exceeding its indexing limits. Floating-point timestamps are not accepted.

Standard output contains one row per vertex:

```text
original_vertex_id score
```

Rows follow the order in which vertex IDs first appear in the input, including
`@vertex` declarations. They are not necessarily sorted numerically.

## Batch benchmarks

`scripts/benchmark.py` runs configurations serially, starts a new process for
each repetition, and records individual runs and aggregate summaries.

For unit transition times:

```bash
python3 scripts/benchmark.py data/graph.tg \
  --orders 32 128 \
  --methods THOR-Basic THOR-NP THOR STREAM \
  --repeats 3 \
  --timeout-s 3600 \
  --output results/uniform
```

Pass multiple graph paths to benchmark multiple datasets. Use `--build PATH`
to select an alternate build directory. By default, the script runs `THOR` at
orders `32` and `128`, with three repetitions and a 3600-second process timeout.
Choose a new or empty output directory for each experiment.

The output includes:

| Output | Contents |
| --- | --- |
| `runs.csv` | One row per graph, order, method, and repetition |
| `summary.csv` | Aggregate timing and memory statistics with completion counts |
| `runs/NNNNNN/metrics.json` | Metrics emitted by an individual successful process |
| `runs/NNNNNN/stderr.txt` | Error messages from an individual process |
| `runs/NNNNNN/answers.txt` | Vertex scores, when `--keep-answers` is requested |

Timeouts and failed runs retain their status. A timed-out process does not
provide a completed algorithm runtime. Summary statistics use successful runs
and are marked `complete`, `partial`, or `no_success`, with the requested and
successful repetition counts. The command exits with a nonzero status if any
configuration fails, times out, or is unsupported.

## Transition-time variants

The preparation script replaces transition times while retaining endpoints,
departure times, vertex declarations, and event multiplicity:

```bash
python3 scripts/prepare_durations.py data/graph.tg \
  --output data/durations \
  --policies U1 U0 U4 M1 M2 H \
  --seed 2027
```

| Policy | Assigned transition times |
| --- | --- |
| `U1` | Always `1` |
| `U0` | Always `0` |
| `U4` | Always `4` |
| `M1` | Approximately uniform over `{0, 1, 2, 4, 8}` |
| `M2` | Approximately uniform over `{1, 2, 4, 8, 16}` |
| `H` | Approximately 90% `1`, 9% `4`, and 1% `64` |

Assignments are deterministic for the same seed and edge event. The mixed
policies do not enforce exact frequencies. Identical duplicate events receive
the same assigned duration. Choose a new or empty output directory. The script
writes a graph such as
`data/durations/M1.tg` for each policy and a `summary.json` containing the actual
transition-time histograms. Durations use the same time unit as the input
timestamps; these policies assign synthetic durations, not observed travel or
interaction times.

Use `--undirected` when existing opposite-direction events at the same departure
time should receive the same duration. This option does not create reverse
edges.

For a nonuniform variant:

```bash
python3 scripts/benchmark.py data/durations/M1.tg \
  --orders 32 128 \
  --methods THOR-Basic THOR-NP THOR Recurs-official \
  --repeats 3 \
  --output results/mixed
```

Use nonnegative departures for comparisons with Recurs-official. STREAM
supports `U1` only.

## Workload profiling

THOR, THOR-Basic, and THOR-NP accept `--stats FILE.csv` for per-order
statistics. THOR additionally accepts `--work-stats FILE.csv` for workload
counters, including scanned vertices and edges, edges skipped by saturation,
changed classes, and dependency visits:

```bash
mkdir -p results/profile
./build/thor data/graph.tg 128 \
  --stats results/profile/orders.csv \
  --work-stats results/profile/work.csv \
  --metrics results/profile/metrics.json > results/profile/answers.txt
```

The instrumented STREAM implementation records list scans and candidate visits:

```bash
./build/stream_profile data/graph.tg 32 \
  --metrics results/profile/stream.json > /dev/null
```

It writes the counters to `results/profile/stream.json.work.json`. Like
STREAM, it requires transition time `1` on every edge.

Profiling performs additional instrumentation and output work. Use ordinary
runs without profiling to report algorithm efficiency.

## Timing and memory

The JSON timing fields have the same meaning across executables:

| Field | Meaning |
| --- | --- |
| `load_s` | Read and parse the input graph |
| `prep_s` | Sort and construct algorithm-specific data structures |
| `solve_s` | Compute the requested-order values |
| `algorithm_s` | `prep_s + solve_s` |
| `total_s` | `load_s + prep_s + solve_s` |
| `peak_rss_bytes` | Process peak resident memory, when available |

Use `algorithm_s` for algorithm-runtime comparisons. Vertex output occurs after
the solve timer stops. The benchmark's `wall_s` also includes process startup
and output handling, so it is distinct from either JSON time total.

For Recurs-official, conversion to the upstream temporal graph, sorting, and
memoization-container initialization are included in `prep_s`. Peak RSS covers
the process, including input loading and preprocessing.

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/` | C++ implementations, shared headers, and the Recurs adapter |
| `src/vendor/` | Bundled Recurs baseline components and compatibility header |
| `scripts/build.py` | Build the executables |
| `scripts/benchmark.py` | Run repeated timing experiments and produce CSV summaries |
| `scripts/prepare_durations.py` | Generate synthetic transition-time variants |
| `examples/example.tg` | Small runnable temporal graph |
| `THIRD_PARTY_NOTICES.md` | Baseline provenance and attribution |

Place datasets under `data/` and generated measurements under `results/`.
Build products, local data, and results are excluded by `.gitignore`.

## References and third-party code

The recursive baseline and the streaming algorithm originate from:

> Lutz Oettershagen, Nils M. Kriege, and Petra Mutzel.
> **A Higher-Order Temporal H-Index for Evolving Networks.** KDD 2023.
> [Paper](https://doi.org/10.1145/3580305.3599242) ·
> [Preprint](https://arxiv.org/abs/2305.16001) ·
> [Authors' code](https://gitlab.com/tgpublic/tgh)

Recurs-official uses the bundled upstream source through a local adapter.
STREAM and `stream_profile` are local reimplementations of the streaming
algorithm. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for details.
