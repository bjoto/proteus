# Test workflow and timing

Multiplayer tests are [planned, not implemented](MULTIPLAYER_PLAN.md#verification-matrix-and-telemetry).
The owning plan specifies seeded impairment profiles, collision/wake consistency gates, process
tests and long-session/four-player stress. Add runnable commands and labels as those targets
land; the current routine suite below does not verify network behavior.

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
stay enabled. With `PROTEUS_GPU_TESTS=ON`, the current normal suite runs 83 checks,
including 20 graphics checks. GPU cases retain serial scheduling while independent CPU
checks may run concurrently; fixture dependencies still govern capture/replay comparisons.

`PROTEUS_OPTIMIZE_TESTS=ON` compiles the long numerical validators at `-O2` even in
Clang/GCC Debug builds: boat handling, banking, turn entry, environment, replay, water
routes and object-wake review. Debug symbols and assertions remain enabled; there is no
fast-math, reduced simulation duration or sampled-down scenario coverage. Application
executables keep their selected build type. For stepping through these validators:

```sh
cmake -S . -B build -DPROTEUS_OPTIMIZE_TESTS=OFF
cmake --build build -j
```

The optimization override currently applies to Clang/GCC only. MSVC keeps its selected
configuration; Windows timings have not been measured. GPU checks are opt-in through
`PROTEUS_GPU_TESTS=ON` and need a desktop context. A display-free suite with GPU checks
off has less coverage; report that omission.

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
