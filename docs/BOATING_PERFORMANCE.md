# Boating performance tools

The foundations [P2a task](BOATING_FOUNDATIONS_PLAN.md#p2a--first-multiplayer-prerequisite-task)
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

Next: bounded simulation benchmark/report contract and full-suite verification.
