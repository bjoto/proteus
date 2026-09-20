# Alternate soundtrack plan

Status: musical direction agreed; composition and tool setup have not started.

This is the working plan for an original Proteus soundtrack. Update it as we listen,
choose directions, and complete milestones. The existing demo timeline does not
constrain the composition; visual scenes will follow the music later.

## Musical brief

Create an approximately four-minute symphonic electronic adventure: epic to the
point of exaggeration, melodic, and dynamic enough to support distinctly different
visual moods. Duration and section timings are provisional.

Use Skaven as a reference for theatrical character, memorable melodies, expressive
sample sounds, rhythmic detail, and playfulness alongside grandeur. Write original
themes. A useful reference is [War in the Middle Earth – Redux](https://soundcloud.com/skaven252/war-in-the-middle-earth-redux),
which Skaven describes as epic synth-rock, sequenced in FL Studio from an earlier
tracker composition.

Core ingredients:

- One recognisable main motif, developed into heroic, fragile, and threatening forms.
- A contrasting lyrical theme introduced in the quiet passage and combined with
  the main theme in the finale.
- Punchy electronic drums and mobile synth bass, with strings, brass, choir, bells,
  and an expressive synth lead.
- Tracker character through articulation, pitch slides, fills, and quick flourishes.
- Two major peaks, with the largest instrumental range and full choir saved for
  the final one. Quiet passages must hold attention on their own.
- Exaggeration through harmony, register, rhythm, and anticipation as well as volume.

## Provisional musical arc

| Approximate time | Movement | Musical role | Mood |
|---|---|---|---|
| 0:00–0:25 | The signal | Distant choir, drone, bells, unresolved motif fragment | Mystery and scale |
| 0:25–0:55 | Awakening | Arpeggio, bass and percussion enter; strings answer | Discovery and motion |
| 0:55–1:30 | First ascent | First full heroic theme with broad lead and driving drums | Confidence and exhilaration |
| 1:30–1:55 | Something goes wrong | Broken rhythm, chromatic harmony, low brass, fragmented theme | Threat and instability |
| 1:55–2:25 | Suspended world | Sparse solo voice, spacious harmony, lyrical theme | Wonder and vulnerability |
| 2:25–2:55 | The gathering | Returning pulse, marching percussion, rising strings | Purpose and anticipation |
| 2:55–3:35 | Apotheosis | Main theme and countermelody, full choir, final harmonic lift | Overwhelming triumph |
| 3:35–3:55 | Afterglow | Decisive ending, opening motif returns resolved | Arrival and closure |

Sections should have clear musical boundaries and meaningful accents for eventual
visual choreography. Phrase lengths, tempo, key, and metre remain composition choices;
the existing 125 BPM XM grid is not a requirement for this alternate track.

## Working method

Work in small, audible iterations. Each milestone produces a listening render and
editable source, followed by a short note recording what worked and what to change.
Use timestamped feedback where possible, such as "0:34: the lead needs more lift."
Update this document with decisions instead of maintaining separate competing plans.

The assistant can prepare arrangement data, MIDI, cue sheets, scripts, and exports
where the available tools permit. Listening feedback determines whether the music
achieves the intended effect. Tool setup and successful rendering alone do not
establish musical quality.

Prefer MIDI composition in a DAW with sampled and synthesized instruments, then
rendered audio for the demo. REAPER is a candidate, not a committed dependency.
Choose the actual authoring and rendering tools after checking local availability.
Keep the arrangement editable and repeatable; MIDI alone does not preserve instrument
patches, effects, or mix settings, so retain those with the project.

## Milestones

### 1. Establish a usable sound palette

- Check available DAW, MIDI/rendering tools, and instrument libraries.
- Choose a small initial palette: lead, harmony, bass, drums, and one atmospheric voice.
- Render a short phrase to confirm the path from editable notes to audible output.
- Record the tools, instrument sources, and exact render procedure here.

Done when: we have a reproducible audio render and enough control to change notes,
instruments, and dynamics without rebuilding the toolchain.

### 2. Make the 60–90-second proof of direction

- Write the main motif and a contrasting lyrical phrase.
- Arrange a quiet passage, a rebuild, and an exaggerated final theme.
- Make the quiet and heroic material recognisably related.
- Export a playable stereo sketch with a brief timestamped listening guide.

Listen for:

- Can we remember the main motif after playback?
- Does the quiet passage feel like music worth hearing by itself?
- Does the transition create anticipation and make the arrival satisfying?
- Is the climax excessive in the enjoyable way we want?
- Does the electronic/orchestral balance have character?

Done when: listening feedback supports the theme, contrast, and palette. Revise this
short sketch before spending time orchestrating the full song if those are weak.

### 3. Sketch the complete composition

- Expand the accepted material into the full arc using a modest instrument count.
- Develop the threatening variation and the first peak.
- Compose transitions and an intentional ending.
- Establish the tempo map, phrase lengths, section names, and major accents.

Done when: the entire piece works as a continuous listening experience, every section
has a distinct mood, and the finale feels larger than the first peak.

### 4. Orchestrate and mix

- Develop articulations, fills, countermelodies, spatial effects, and sound design.
- Preserve melodic clarity and the difference between intimate and massive passages.
- Check the complete render for clipping, accidental clicks, and obscured melody.
- Audition on headphones and speakers where available; record any listening limitations.

Done when: a complete stereo render holds up without visuals and has the intended
dynamic range. Keep the editable project and instrument settings with the revision.

### 5. Prepare demo integration

- Choose the playback format based on the accepted music and runtime requirements.
  A stereo audio file plus explicit cues is the default proposal; XM is optional.
- Export section boundaries, tempo changes, and important accents with stable names
  and times measured from the start of the final audio render.
- Keep aligned stems for drums, bass, harmony, lead, and atmosphere if useful for
  iteration. Runtime stem mixing is only needed if we later want adaptive playback.
- Design visual scenes around the musical moods and transitions.
- Implement playback and synchronization, then verify start, seek, scene jumps,
  spectrum analysis, and ending behavior against the soundtrack.
- Update controls/workflow documentation and run the project build, CTest, and a
  manual audiovisual demo check when integration changes code or playback.

Done when: the demo follows the accepted composition and musical cues stay aligned
through playback and seeking.

## Artifact handling

Use a separate alternate-soundtrack workspace in the repository when composition
begins. Keep source, instrument provenance/settings, and render instructions together.
Use revisioned listening renders so comparisons remain possible. Put disposable
renders in `build/` and add chosen delivery assets under `assets/music/` later.
Record any external libraries needed to reproduce the sound.

The starter XM and its generator remain available for comparison. This exploration
does not require regenerating or replacing them.

## Progress and decisions

- [x] Agree the musical direction and initial arc.
- [x] Create this working plan.
- [ ] Establish tools and palette.
- [ ] Produce and assess the short proof of direction.
- [ ] Complete the full composition sketch.
- [ ] Complete orchestration and mix.
- [ ] Integrate the accepted soundtrack with the demo.

Next action: inspect the locally available composition/rendering tools and instrument
libraries, then choose the simplest reproducible setup for the short sketch.

Open choices: authoring tool, instruments, tempo/key, final duration, and delivery
format. Resolve them when their milestone needs them.

For each listening revision, append: render location, changes, listening feedback,
decision, and next action. No audio revisions have been produced yet.
