# Capsize deathmatch: multiplayer game mode plan

Prepared 2026-10-24 against `7a072c6`. **D1, the authority-side match core, is implemented;
clients do not yet see scores.**
This document owns the deathmatch rules, phases, verification and acceptance. The
[multiplayer plan](MULTIPLAYER_PLAN.md) owns transport, prediction and room hosting; the
[physics](BOAT_PHYSICS_PLAN.md) and [wake](WAKE_REWRITE_PLAN.md) plans own handling and wake
feel, including tuning of the new mechanics below.

Status: D1 (match core, `boat_server --mode deathmatch`) implemented and automatically verified
on 2026-10-24, not user-played; see the [D1 handoff](#d1-match-core-handoff--2026-10-24). Next:
D2, protocol v2 and the in-game HUD. D3–D6 are pending. The M4 manual acceptance session
continues in parallel by the user's choice; the deathmatch does not wait for it.

## Product decision

On 2026-10-24 the user chose to give multiplayer a purpose: a timed deathmatch in which players
score by capsizing each other. This supersedes the earlier "no scoring in the first release"
default in the multiplayer plan's product boundary. Decisions recorded with the user:

| Question | Decision |
| --- | --- |
| Round structure | Timed rounds (default 5 min), scoreboard intermission, then a new round |
| Capsizing yourself | −1 point |
| New capsize mechanics | All four, in order: side-ram roll, wake surge, swell mine, shrinking arena |
| Sequencing | Start now, in parallel with M4 manual acceptance |

Free mode (current shared free boating) stays the default room mode and must remain unchanged
byte for byte; deathmatch is a room option chosen by the server.

## Rules

The authority alone decides credit and scores. Clients display confirmed state and never
predict credit.

**Touches.** Each boat keeps one most-recent touch `{slot, tick, cause}` from another slot:

- *Contact*: the pair's accumulated impulse in `boat_fleet_solve` is at least 1.5 normalized
  units (initial guess; tune in D1). Both boats record a touch by the other.
- *Wake*: another emitter's records under the hull sum to at least 0.35 m of |height|, or that
  emitter has the largest share on the tick the boat becomes `BOAT_AIRBORNE`. A boat's own
  emitter never counts.
- *Later causes*: `SURGE` (D4), `MINE` (D5) and `ARENA` (D6).

**Credit.** When a boat first latches `BOAT_CAPSIZED` (the existing rule: 0.6 s inverted in
water, `motion_step` in `src/boat.c`), the latest touch within a 5 s credit window scores +1
for its slot. Without such a touch the victim scores −1 (self-capsize). Rows track score,
capsizes caused, times capsized and self-capsizes.

**Bailout.** R while upright within the credit window after a touch counts as a capsize
credited to the toucher; R otherwise is free.

**Respawn.** After a confirmed capsize the authority requests a spawn itself after 3 s; R
skips the remaining wait after 1 s. Spawns use the existing `boat_fleet_request_spawn` safe
spawn rules. A new life has a 2 s credit grace: touches by or against it are not recorded.
This is not physical invulnerability, which the multiplayer plan rules out.

**Rounds.** `WARMUP` (fewer than two players: free play, no scoring) → `PLAYING` (300 s,
`--round-seconds`) → `INTERMISSION` (10 s scoreboard; boats keep moving) → scores reset and
every slot respawns → `PLAYING`. Mid-round joiners start at zero; a leaver's row is removed and
pending credit to them is dropped. Ties share first place.

## Architecture

- `src/boat_match.[ch]` (new, `proteus_boat_core`, obeys the core boundary): `BoatMatchRules`
  (round, intermission, window, thresholds, grace, respawn delay, mechanic toggles) and
  `BoatMatch` (phase, phase end tick, score rows, last touches, lives and latches seen,
  respawn deadlines). `boat_match_observe` runs after each `boat_fleet_step`, returns bounded
  events (`CAPSIZE{victim, attacker|none, cause}`, `ROUND_START`, `ROUND_END`) and spawn
  requests for the server to apply. It never changes physics, the fleet hash or prediction.
- `src/boat_fleet.[ch]`: diagnostic `pair_impulse[BOAT_FLEET_PAIRS]` in `BoatFleetStats`,
  filled from the solver's accumulated impulses; physics never reads it.
- `src/boat_wake_objects.[ch]`: read-only `boat_wake_objects_query_emitter`, the ordinary
  evaluation restricted to one emitter's 32 slots.
- `src/boat_net_server.[ch]`: room mode and rules in `BoatNetServerConfig`, a `BoatMatch`
  beside the fleet, automatic respawns and bailouts through the spawn request handler.
- Protocol v2 ([protocol](MULTIPLAYER_PROTOCOL.md)): reliable control messages
  `MATCH_STATE` (mode, phase, phase end tick, round, per-slot rows; also sent during bootstrap
  for join-in-progress) and `MATCH_EVENT` (tick, kind, victim, victim life, attacker, cause),
  with golden, round-trip, truncation and invalid-field fixtures.
- `src/boat_net_client.[ch]`: latest match state and a six-entry event ring.
- `tools/boat_study.c`, `src/boat_hud.c`: in deathmatch only, round timer and own score, a
  four-line kill feed (5 s per entry, roster names), a Tab-held scoreboard also shown during
  intermission, and `CAPSIZED BY <name> / RESPAWN n`. Free mode HUD unchanged.
- `tools/boat_server.c`: `--mode free|deathmatch`, `--round-seconds`, logged match events and a
  scripted deathmatch run. `tools/boat_bot.c`: a `--hunt` script that lays wakes across and
  rams the nearest boat.

## Phases

Every phase ends with a warning-free build, the full active CTest suite and an unchanged
offline `boat_study`.

1. **D1 — match core.** `boat_match`, the impulse diagnostic, the per-emitter query, server
   mode, automatic respawn, bailout and scripted server output. New
   `tools/validate_boat_match.c` (core label): synthetic credit-window edges, self −1, bailout,
   grace, leaving attacker, round transitions and ties; physical fleet cases where A's wake
   capsizes B (credited `WAKE`), an own-wake capsize (−1) and a contact touch expiring after
   5 s. Fleet/replay traces and single-player captures stay identical.
2. **D2 — protocol v2 and HUD.** Messages, client state, kill feed, timer, scoreboard,
   intermission and `boat_bot --hunt`. Extend `validate_boat_net` and add a loopback
   deathmatch smoke (server plus two hunting bots for one short round; both clients' scores
   equal the server log). Inspect CPU/GPU HUD captures. Manual: user against bots, then two
   humans.
3. **D3 — side-ram roll.** A contact also applies a roll impulse to the struck boat, scaled by
   lateral impulse and contact height above its centre of mass, so a fast T-bone can tip it.
   Room rule `ram_roll` in the checkpoint and content digest, off in free mode, so single
   player and the crossing rival stay unchanged. Gate: glancing hits never capsize; a square
   T-bone above a named speed does. Extend prediction-replay identity tests.
4. **D4 — wake surge.** `BoatInput.surge` and a command flag bit; a checkpointed, hashed meter
   in `Boat`: 1.5 s of about ×2 crest strength, then 8 s recharge. Records already carry
   amplitude, so births replicate unchanged; confirm the protocol range envelope accepts the
   taller crests. HUD meter, an unused key, cause `SURGE`.
5. **D5 — swell mine.** Starts with a feasibility spike recorded here. A buoy dropped at the
   stern (one per player, recharging) bursts after 1.5 s into a radial ring wave: a ring record
   kind or a small separate pool, CPU query, GPU pack/shader parity, protocol births,
   checkpoint and a small buoy mesh. Cause `MINE`.
6. **D6 — shrinking arena.** Match state carries an arena centre and a radius that is a
   function of the round tick. Outside it a deterministic storm swell, a function of tick and
   position only, is added in `boat_water_query` with a matching GPU uniform, so prediction
   stays exact. Buoy ring, HUD warning, cause `ARENA` (−1 unless another touch is in the
   window).

Each mechanic gets a user feel gate before it is on by default in deathmatch rooms.

## Verification and acceptance

- Build and `ctest --test-dir build -j 4 --output-on-failure`, warning-free, at every phase.
- Free mode byte identity: Release worktree before/after diffs of fleet and replay validator
  output, P3a traces and single-player CPU captures.
- D1: `validate_boat_match` and the scripted `boat_server --mode deathmatch` event log.
- D2: loopback deathmatch smoke, inspected CPU/GPU HUD captures, an agent windowed run against
  `boat_bot --hunt`, then user play.
- D3–D6: validator cases, prediction-replay identity, captures and a user feel gate.

Acceptance for the mode: two or more humans finish several rounds where credits match what
they saw, self-capsizes feel fair, nobody can deny a capsize by resetting, and the round loop
runs without a room restart. Record only checks actually performed, with dates and platform,
in dated handoff sections below.

## Handoffs

### D1 match core handoff — 2026-10-24

Implemented:

- `src/boat_match.[ch]` in `proteus_boat_core`: `BoatMatchRules` (defaults as in the rules
  above), `boat_match_observe`, `boat_match_reset` and `boat_match_rank`. Presence is derived
  from the fleet (active or pending), lives from `BoatFleetSlot.life`.
- `BoatFleetStats.pair_impulse`, the solver's accumulated impulse per pair (diagnostic), and
  `boat_wake_objects_query_emitter`, the ordinary record evaluation over one emitter's slots.
- `BoatNetServerConfig.deathmatch`/`.match`: the server observes the match after every tick,
  queues its spawns, routes `SPAWN_REQUEST` through `boat_match_reset` and keeps the last 16
  events for the caller. No protocol change: clients see automatic respawns as new lives, but
  their HUD still says `CAPSIZED / R TO RESTART` and shows no score.
- `boat_server --mode free|deathmatch --round-seconds S` (rooms and `--scripted`), logging
  round starts, capsizes with attacker and cause, round-end scoreboards and abandoned rounds.
  Scripted deathmatch bots leave recovery to the match.

Deviations and choices:

- Capsizes outside `PLAYING` (warmup, intermission) still mark the boat out and respawn it but
  are neither scored nor reported. A round below `min_players` is abandoned to warmup.
- A round start respawns every present boat, including the one already playing in warmup.
- Wake touches compare one emitter's |height| at the hull centre (`Boat.position`), not at the
  six support points; the launch rule credits the strongest emitter there on the tick the boat
  becomes airborne.
- Contact and wake thresholds (1.5, 0.35 m) are untuned first guesses.

Verification, 2026-10-24, M1 Max/macOS 26.6.2, Debug:

- Build warning-free in project sources; all 108 active CTests pass, including the new
  `boat_match`, `boat_server_scripted_deathmatch` and a rejected `--round-seconds` without
  deathmatch; the core and net boundary checks pass with `boat_match` in the core.
- Free mode unchanged: `boat_server --scripted` output for 2400 and 18000 ticks (fleet hashes,
  wake, contact and life counters) is identical before and after the change.
- Scripted 18000-tick deathmatch, 60 s rounds, four bots: two complete rounds and a third
  started; 21 capsizes, all credited (20 `WAKE`, 1 `CONTACT`), no self-capsizes, no emission
  failures.
- ENet room on loopback with two `boat_bot` clients for 45 s, 20 s rounds: the round started
  when the second bot joined, a wake capsize was credited, round 1 ended with its scoreboard and
  round 2 was abandoned as the bots left.

Not verified: no human play, no `boat_study --connect` session against a deathmatch room, no
Windows/Linux build. That bots' capsizes are all credited suggests the wake threshold may be
generous; credit fairness needs human play (D2).
