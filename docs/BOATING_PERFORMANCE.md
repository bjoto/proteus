# Boating performance tools

The foundations [P2a task](BOATING_FOUNDATIONS_PLAN.md#p2a-completion--2026-10-23)
owns the finite scripted arcade wake-return benchmark. Full P2 renderer counters and the
P3 workload matrix remain pending.

## Shared driver extraction — 2026-10-23

`tools/boat_study_drive.[ch]` now owns the app's existing scripted controls and step wrapper.
It preserves advance → boat step → stern track, float time arithmetic, optional D3/field
paths and the rival's existing route/contact/reset sequence. There is no fleet scheduler.
`tools/boat_drive_baseline.h` freezes the original driver from `2994e0d` for regression checks.

Apple Silicon/macOS, Debug: `boat_drive_extraction` matches named physical/wake hashes at
all 3840 ticks against that driver and matches CPU pixels at ticks 1080 and 2400.
App captures `build/bench/before9.png`/`after9.png` and `before20.png`/`after20.png`
match byte for byte and were inspected. A desktop `boat_study --demo --frames 180` run
completed (378 physics ticks). This is scripted playback evidence, not human handling or
audio acceptance. The extraction changes no controls or intended simulation behavior.

## Report contract

Run from the repository root after the normal Debug configure/build:

```sh
mkdir -p build/bench
./build/boat_benchmark --scenario wake-loop --mode simulation --renderer none --warmup 120 --frames 1800 --output build/bench/arcade-baseline
```

Only scenario `wake-loop`, mode `simulation`, and renderer `none` are supported (also the
defaults). `--output` is required: it is a file prefix, with an existing parent directory;
the tool writes `<prefix>.json` and `<prefix>.csv`. Existing reports are overwritten.
`--warmup` defaults to 120 and accepts 0..100000 nominal frames; `--frames` defaults to
1800 and accepts 1..100000. Unknown options, invalid values, allocation and output errors
return nonzero. Count bounds precede size/tick arithmetic; all timing storage is preallocated.
No window, audio device, replay/course storage or personal-best writes are involved.

Each nominal frame advances exactly two existing 120 Hz ticks. Warmup and measurement form
one continuous drive. Steering is commanded from tick 420 through 1439; at tick 1440 the
script commands neutral throttle and steering. This is a finite scripted wake-return fixture,
not sustained powered driving or a full-pool workload. Births, peak objects and landings cover
the entire run, including warmup. The pool remains 128 slots, 32 per emitter; one emitter is used.

JSON schema/scenario/hash revisions are currently 1. The report identifies the build's configure-time
Git revision (with `-dirty` for tracked changes), build type/compiler, platform, selected options,
warmup/measured/total tick counts, wake lifetime/height and observed births/peak/landings.
`simulation_ms` contains p50/p95/p99/max over **all measured frames**, using sorted index
`floor((frames - 1) * percentile / 100)`. CSV columns are zero-based `frame`, absolute
`end_tick`, and `simulation_ms`. The values use `boat_profile_time(false)` and include the
existing shared tick wrapper. `render_cpu_ms` and `gpu_ms` are null because they are unavailable.
The report is written and samples sorted after all ticks; timing excludes hashing and file I/O.

`state_hash` folds FNV-1a over named numerical `%a` values after every nominal frame, including
warmup. `boat_study_state.c` lists its contract: physical pose, velocities, controls, motion state,
motion timers/landing counter, physical wake bookkeeping, world tick/emission/expiry counters,
all object records and all emitter fields. It excludes raw structs, padding, pointers and cosmetic
particles/flag/camera state. This is same-build deterministic evidence, not a collision-proof state
comparison, portable replay format or claim of cross-platform bit identity. CPU pixel comparisons
add coverage for the extracted app path.

## P2a completion evidence — 2026-10-23

Apple Silicon/macOS, Debug, `PROTEUS_GPU_TESTS=ON`, default retired field tests disabled:

- Normal configure/build pass without compiler warnings (raylib emits its existing macOS OpenGL
  deprecation CMake notice).
- `ctest --test-dir build -j 4 --output-on-failure` passes all 85 active tests, including
  20 GPU cases, in 113.79 seconds with desktop access (`build/bench/ctest-desktop.log`).
  Fifteen retired field cases remain disabled. The initial sandbox attempt stalled on the first
  GPU case and was stopped; it is not counted as a passing full run.
- The final focused driver/benchmark checks pass after the metadata and output-error updates.
  Benchmark checks cover schema, all CSV rows/percentiles, repeat hashes, warmup continuity,
  one-frame/zero-warmup boundaries, malformed/overflow/unsupported options and output failures.
- Two 120-warmup/1800-measured runs agree on state hash `8bca0cb6394cc24f`, 3840 total
  ticks, 17 births, peak 17 objects and one landing. Reports are
  `build/bench/arcade-baseline.{json,csv}` and `build/bench/arcade-repeat.{json,csv}`;
  build identifier `b06ecd2-dirty/Debug/AppleClang-21.0.0.21000101`.
  Observed simulation p50/p95/p99/max were 0.327/0.562/0.595/0.735 ms and
  0.282/0.567/0.586/0.673 ms. These are smoke measurements, partly concurrent with verification,
  not a controlled P3a performance baseline.

P2a is implemented. Next is P3a: capture the current arcade baseline under controlled conditions,
with the required representative CPU/GPU evidence before clock migration. Full P2/P3 remain
pending. No new physics tolerance, pixel baseline, handling change or user acceptance is claimed.
Windows/Linux/browser qualification and human motion/audio review were not performed.

## P3a trace preparation — 2026-10-23

The benchmark now additionally writes `<prefix>.state.csv` after all simulation ticks.
Its columns record every nominal frame, including warmup: absolute end tick, accumulated float
simulation time, X/Z position and velocity, height/vertical velocity, motion state, landings,
live wake objects, births and the rolling physical/wake hash. Storage is bounded and allocated
before warmup; copying these values happens after the timer stops. Existing JSON/timing CSV
formats and scenario/hash revisions remain unchanged. The new trace is diagnostic output,
not a change to the simulation or a portable replay contract.

The contract test checks trace row counts, finite values, end ticks, report agreement and exact
repeat traces, plus failure to open a trace output. `boat_benchmark_baseline.py` captures three
sequential 120-warmup/7200-measured runs and requires matching traces and the fixture's actual
17 births/peak, one landing, flight/landing states and eventual complete expiry.
P3a remains in progress until controlled measurements and CPU/GPU evidence are recorded below.
