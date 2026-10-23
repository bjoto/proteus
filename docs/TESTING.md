# Test workflow and timing

Multiplayer M2's local authority has display-free checks (`boat_fleet`, `boat_server_scripted`,
see [below](#core-boundary-gate)). Network tests are
[planned, not implemented](MULTIPLAYER_PLAN.md#verification-matrix-and-telemetry): seeded
impairment profiles, collision/wake consistency gates, process tests and long-session/four-player
stress. Add runnable commands and labels as those targets land; the routine suite does not
verify network behavior.

Status, 2026-10-22: the user requested a quick routine suite and suspension of the
retired physical wake-field tests until that research resumes. Arcade wakes, handling,
course/replay, environment, audio and renderer checks remain active.

## Routine checks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build -j 4 --output-on-failure
```

`PROTEUS_WAKE_FIELD_TESTS` defaults to `OFF`. Fifteen registered experiments are marked
disabled, covering the owned field, refinement, cadence, candidate sources/boats/GPU,
compact packets and Python prototypes. CTest lists them as disabled, not failed. Their
sources, targets and manual diagnostic commands remain available. All arcade-object tests
stay enabled. With `PROTEUS_GPU_TESTS=ON`, the normal suite runs 93 checks as of 2026-10-23
(after multiplayer M2; counted from the generated CTest file). GPU cases retain serial scheduling while independent
CPU checks may run concurrently; fixture dependencies still govern capture/replay comparisons.

Every active test carries one of three labels: `cpu` (49, display-free), `render` (23, CPU
captures and checksums) or `gpu` (21, graphics context). The five `core` checks are also `cpu`:
`boat_core` runs `validate_boat_core`, which links only the boating core library;
`boat_core_boundary` runs `tools/check_core_boundary.py` over the core sources and archive;
`boat_fleet` runs `validate_boat_fleet` (multiplayer M2 fleet step, contacts, spawns and
checkpoints); and `boat_server_scripted`/`boat_server_unscripted_rejected` run the scripted
authority for 2400 ticks and check that it refuses to start without `--scripted`.
Select a group with `-L`, for example `ctest --test-dir build -L core`; CTest adds fixture
setup tests that a selected test requires, whatever their label.

`PROTEUS_OPTIMIZE_TESTS=ON` compiles the long numerical validators and the shared boating
libraries (core, render, session and drive) at `-O2` even in Clang/GCC Debug builds. The
validators are boat handling, banking, turn entry, environment, replay, clock, world isolation,
core, fleet, scripted server, water routes and object-wake review. Debug symbols and assertions remain enabled; there
is no fast-math, reduced simulation duration or sampled-down scenario coverage. Because
validators and applications link one copy of each library, Debug `boat_study` also runs this
optimized boat code; the GPU library and application sources keep the selected build type. For
stepping through these validators or library code:

```sh
cmake -S . -B build -DPROTEUS_OPTIMIZE_TESTS=OFF
cmake --build build -j
```

The optimization override currently applies to Clang/GCC only. MSVC keeps its selected
configuration; Windows timings have not been measured. GPU checks are opt-in through
`PROTEUS_GPU_TESTS=ON` and need a desktop context. A display-free suite with GPU checks
off has less coverage; report that omission.

## Core boundary gate

`proteus_boat_core` holds physics, pose, locations, world and shoreline queries, water queries,
wakes, course crossing, harbor/traffic contacts and boat/city geometry data. The boundary check
rejects any core include outside the core headers, `raylib.h`, `raymath.h`, compiled map
geometry and a short system-header list without `<stdio.h>`. It also rejects raylib calls
other than raymath and C library console, file and process calls. When `nm` is available
(not MSVC), every undefined symbol in the core archive must be defined in core or be raymath,
libm, libc memory/sort/assert, `clock()` or the Accelerate DFT backend. The script first runs
a self-test that each rule rejects a known violation:

```sh
python3 tools/check_core_boundary.py --self-test
```

Add a new core source and its header to `PROTEUS_BOAT_CORE_SOURCES` in `CMakeLists.txt`.
Presentation, GPU, session/replay and the boat_study drive/profile code live in the
`proteus_render`, `proteus_gpu`, `proteus_boat_session` and `proteus_boat_drive` libraries.

## Long-session clock gate

`boat_clock` checks direct-time cases (one day, 30 days) without simulating the preceding
ticks; `boat_clock_soak_smoke` runs a 60 second wave soak. The actual one-hour soak is opt-in
and should use a Release build; it took about 95 s CPU on an M1 Max on 2026-10-23:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target validate_boat_clock -j
mkdir -p build/bench/c3
./build/release/validate_boat_clock --soak 3600 build/bench/c3
./build/release/validate_boat_clock --soak 3600 build/bench/c3 --waves
```

The soak accepts 60..86400 seconds and writes CPU frames at one minute, halfway and the end.
Its printed hash is same-build evidence, not a cross-platform contract.

## Revisit the physical field

```sh
cmake -S . -B build -DPROTEUS_WAKE_FIELD_TESTS=ON
cmake --build build -j
ctest --test-dir build -L wake_field --output-on-failure
# Return to the normal suite afterwards:
cmake -S . -B build -DPROTEUS_WAKE_FIELD_TESTS=OFF
```

The label includes only retired research tests, not the active arcade model. This is
suspended verification of the opt-in field implementation, not evidence that the field
meets its original performance or numerical acceptance goals.

## Measurements and verification

2026-10-22, M1 Max/macOS, Debug, GPU tests enabled. The original serial suite passed
all 98 tests in **748.21 s (12 min 28 s)**. The fifteen retired checks used **434.33 s**;
`wake_candidate_moving_source` alone used **247.92 s**. The remaining checks summed to
313.90 s, including 83.91 s banking and 40.15 s handling in unoptimized Debug builds.
This explains why disabling only the field would still leave a five-minute serial suite.

The revised routine command passes **all 83 active tests in 97.40 s (1 min 37 s)**,
including **20 active GPU checks**: an **87% wall-time reduction** from the old serial
suite. This combined improvement includes field suspension, optimized numerical tests
and four CPU jobs; it is not an isolated compiler benchmark. Banking drops from 83.91 s
to 23.00 s and handling from 40.15 s to 11.01 s despite concurrent CPU work. The largest
remaining check is `gpu_review` at 23.99 s; active GPU checks total 60.31 s. These are
single local runs, not cross-platform guarantees.

Debug configure/full build passes without project warnings. Registration checks confirm
exactly fifteen disabled research cases and all 83 remaining checks enabled; a separate
configure with `PROTEUS_WAKE_FIELD_TESTS=ON` enables all fifteen research cases. The
CTest GPU label summary counts three disabled field cases too (23 listed), so the actual
active GPU count is twenty. CMake-generated validator flags retain `-g` with `-O2`,
without `NDEBUG` or fast-math. No wave recipe or gameplay physics changed here.

Next: keep the routine suite active; revisit field verification only when physical-field
work resumes. Timings and suite logs are in `build/test-speed/`. The prior default-promotion
run also completed before the suite policy changed, so its all-98 result remains valid
historical evidence. New manual motion/audio review was unnecessary for these test-only
changes; the preceding default switch has its own capture/desktop evidence.
