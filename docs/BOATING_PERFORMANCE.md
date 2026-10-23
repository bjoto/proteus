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

At the P2a handoff, P3a was next: capture the current arcade baseline under controlled conditions,
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
This was an intermediate checkpoint; the controlled result is recorded below.

## P3a current arcade baseline — 2026-10-23

P3a is implemented for the finite `wake-loop` simulation fixture. Reproduce the controlled
measurement from the repository root after other test/build jobs finish:

```sh
cmake -S . -B build/p3a-release -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_RAYLIB="$PWD/build/_deps/raylib-src"
cmake --build build/p3a-release --target boat_benchmark boat_study -j
python3 tools/boat_benchmark_baseline.py build/p3a-release/boat_benchmark build/bench/p3a
```

This was run sequentially on a MacBookPro18,2 (Apple M1 Max, 64 GiB), macOS 26.6.2
(build 25G83), connected to AC with `pmset` AC `powermode=2`. The Release build uses
AppleClang 21.0.0.21000101, `-O3 -DNDEBUG`; configure-time identifier is
`e0440b2-dirty` because tracked documentation edits existed before the task. No benchmark
runs overlapped the build or CTest. Other system background activity and thermal state were
not measured. Times are development-machine observations, not hardware certification.

Each run has 120 warmup plus 7200 measured nominal frames: 14,640 ticks total, with
14,400 measured ticks. All 7,320 trace rows match across three sequential runs, as do the
final hash `c29e5d3d13d462c3`, 17 births, peak 17 live objects, one landing and eventual
zero live objects. The trace records airborne at tick 1118, landing at 1328, capsize at 1450
and wake expiry after tick 3468. At tick 14,640, accumulated `Boat.time` is 121.992271 s;
the nominal 122.0 s illustrates the float clock drift that C1/C2 must address.

| Run | Simulation p50 | p95 | p99 | max | Units |
| --- | ---: | ---: | ---: | ---: | --- |
| 1 | 0.065 | 0.136 | 0.158 | 0.250 | ms per two-tick frame |
| 2 | 0.065 | 0.136 | 0.156 | 0.249 | ms per two-tick frame |
| 3 | 0.065 | 0.136 | 0.155 | 0.189 | ms per two-tick frame |

The fixture stops powered turns at tick 1440, capsizes soon afterward and has no live wakes
after tick 3468. Thus 5585 of 7200 measured frames (77.6%) are after wake expiry. From
run 1, the 1615 measured frames through tick 3470 have p50/p95/p99/max
0.070/0.157/0.164/0.250 ms; the 5585 later frames have
0.064/0.067/0.075/0.154 ms. This baseline represents the specified finite return,
not sustained full-pool simulation. It does not establish the complete P3 workload matrix or
render/GPU timing; those fields remain `null` in the benchmark JSON.

Raw reports and trace are `build/bench/p3a/run-{1,2,3}.{json,csv,state.csv}`. The runner's
comparison output is `build/bench/p3a-runs.log`; `build/bench/p3a/sha256.json` records hashes
of those files and the images below. These ignored build artifacts are retained locally;
reproduction commands and the final state hash provide the durable baseline contract.

Representative Release captures were inspected at 320x200:

| Local artifact | Evidence |
| --- | --- |
| `build/bench/p3a/views/{cpu,gpu,gpu-aa}-t9.png` | Wake approach and visible return path; GPU AA smooths edges |
| `build/bench/p3a/views/{cpu,gpu,gpu-aa}-t11.png` | Airborne hull and wake encounter |
| `build/bench/p3a/views/{cpu,gpu,gpu-aa}-t20.png` | Capsized/settled hull and remaining wake |
| `build/bench/p3a/views/cpu-t60.png` | Wake-free later state |

CPU and GPU use different water/presentation paths, so these images are representative views,
not pixel-equality baselines. The capsize and long settled portion are observed fixture behavior;
there is no new visual or handling acceptance claim. A scripted Release
`./build/p3a-release/boat_study --demo --frames 180` desktop run completed 376 ticks. No
independent human control/audio review was performed.

Debug `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` and `cmake --build build -j` pass.
`ctest --test-dir build -j 4 --output-on-failure` passes all 85 active cases, including
20 GPU cases, in 114.10 s with desktop access (`build/bench/p3a-ctest.log`); fifteen retired
physical-field cases remain disabled. The focused trace/schema and frozen-driver checks also
pass. Release raylib compilation emitted one bundled `stb_vorbis.c` tautological-comparison
warning; project sources remained warning-clean. Windows/Linux/browser results were not
collected for P3a.

The multiplayer prerequisite track proceeds to C1, followed by C2/C3 before world/session
extraction. Full P2 renderer counters and remaining P3 scenarios stay pending.

## C1 clock comparison — 2026-10-23

The C1 Release smoke command below uses the same 120-warmup/7200-measured finite arcade fixture
as P3a, after building the changed sources. It is a behavior comparison; C3 will establish the
replacement performance baseline after the remaining time consumers migrate.

```sh
cmake -S . -B build/p3a-release -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_RAYLIB="$PWD/build/_deps/raylib-src"
cmake --build build/p3a-release --target boat_benchmark boat_study -j
mkdir -p build/bench/c1
./build/p3a-release/boat_benchmark --scenario wake-loop --mode simulation --renderer none \
  --warmup 120 --frames 7200 --output build/bench/c1/wake-loop
```

The C1 trace at `build/bench/c1/wake-loop.state.csv` has the same 7320 rows, births 17,
peak 17, one landing, motion transitions at ticks 1118/1328/1370/1450 and last live wake
at tick 3468 as P3a. Every CSV physical/wake column except `time` and the rolling hash matches
P3a at every row. C1 ends at exactly 122 s versus P3a's accumulated 121.992271 s; its
rolling hash is `24d582aae45971b4`. The one-run p50/p95/p99/max of
0.063/0.140/0.152/0.243 ms are diagnostic only, collected on the same M1 Max/AC development
machine, not a controlled replacement baseline.

Release CPU and GPU captures at 9, 11 and 20 s in `build/bench/c1/` match the corresponding
P3a PNG files byte for byte and were inspected. A scripted Release windowed demo and basin
course run of 180 frames each completed (379 and 376 ticks respectively). No human handling,
focus, pause or audio listening acceptance is inferred. At C1, the water/traffic and shader
time consumers still narrowed to float; the C2 comparison below records their migration.

## C2 time-consumer comparison — 2026-10-23

C2 carries double world time into water, traffic, scene actors and shader payloads (see the
[C2 handoff](BOATING_FOUNDATIONS_PLAN.md#c2-handoff--2026-10-23)). The same Release fixture
was run three times after building the changed sources:

```sh
cmake --build build/p3a-release --target boat_benchmark boat_study harbor_review -j
mkdir -p build/bench/c2
for run in 1 2 3; do
  ./build/p3a-release/boat_benchmark --scenario wake-loop --mode simulation --renderer none \
    --warmup 120 --frames 7200 --output build/bench/c2/run-$run
done
```

All three `build/bench/c2/run-{1,2,3}.state.csv` traces are byte-identical to
`build/bench/c1/wake-loop.state.csv`: 7320 rows, final time 122 s, hash `24d582aae45971b4`.
That is expected because this fixture uses arcade wake objects (already age-packed) and no
ambient waves or D3 trail. Simulation p50 is 0.064 ms in each run; p95/p99/max are
0.133/0.147/0.199, 0.129/0.146/0.255 and 0.131/0.147/0.228 ms. These are diagnostic
same-machine observations, not the C3 replacement baseline.

Release `boat_study --renderer cpu|gpu --time 9|11|20 --screenshot ...` captures in
`build/bench/c2/` match C1 byte for byte, except `gpu-t11.png`: two pixels differ by one
level. The basin's ambient amplitudes are zero there; the difference is GPU rounding from the
restructured shader expression, not a time change. Traffic captures from
`./build/p3a-release/harbor_review --renderer cpu|gpu --view traffic --waves --time T` at
T = 40, 86400.5 and 2591999.5 s (`build/bench/c2/{cpu,gpu}-traffic-*.png`) were inspected:
vessels, trails, waves and reflections remain coherent at 24 hours and 30 days.

The water path no longer copies `BoatWater` for every internal query. On the same M1 Max, a
Release CPU `boat_study --location eriksberg --waves --player-wake --time 60` screenshot
run took 1.29 s versus 1.48 s for `6500a65`; the default basin run took 0.33 s versus
0.35 s. These are wall-clock observations that include startup and one render.
