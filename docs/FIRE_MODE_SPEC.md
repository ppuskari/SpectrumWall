# SpectrumWall — Fire mode implementation design (proposal)

Status: **first Fire mode implemented** on
`feature/fire-heatfield-20261127`. The existing LED / Fluid Lines / Hybrid
modes, WASAPI capture, 4096-point stereo FFT, calibrated dBFS scales and
instrumentation are unchanged. This file retains implementation targets
and future refinement opportunities, rather than claiming every idea
below is already implemented.

## Implemented first-pass behavior

- `V` now cycles **LED → Lines → Hybrid → Fire**.
- `--visual fire` is supported; `visual=3` saves/restores safely.
- `src/FireHeat.hpp` holds a deterministic fixed-30 Hz, stereo-isolated
  heat simulation with a maximum **320 × 192** pixel field per pane.
  Upward advection, lateral turbulence, smoothed adjacent FFT envelopes,
  amplitude gating and red→white-hot palette create continuous tongues.
- `src/WallFire.cpp` uploads a streaming RGBA texture only when thermal
  state changes, uses linear sampling, and draws an optional soft halo.
  Fire does not reuse LED segments, peak-hold traces, curve glow or bar
  reflections.
- All three usual displays keep their independent spectrum sources and
  frequency orientation; center L/R halves remain separate. The optional
  fourth waterfall is unchanged.
- Foreground `P` particles are still optional and emit as warm embers
  while Fire is active. The **E** engineering layer, **J** balance gauge,
  spectrum mouse probes, calibrated lower level meters, and F1 popup
  remain above the fire field.
- No traditional peak hold lines are drawn in Fire. Waveforms and
  calibrated lower meters below the flame plots are retained.

**Implemented after first hardware feedback:** three selectable color
palettes, optional short and held animated tip sprites, flame-ridge
lines, configurable thermal speed/texture quality, a decorative pilot
flame backing for calibrated RMS/peak meters, and an optional fourth
monitor stereo Fire view with engineering probes.

**Not implemented by design:** a compute-shader/full-fluid solver or
a separate dedicated flame-waterfall visualization. The low-res heat
simulation and independently optional fourth stereo display meet the
current visual/performance goals without requiring another graphics API.

## New live controls and persistence

| Key | Fire-only operation | Default |
|---|---|---|
| X | Classic / Blue Gas / Plasma palette | Classic |
| Y | Peak-tip Off / Flash / Hold | Off |
| ; | Continuous fire ridge overlay | Off |
| Z | Slow / Normal / Fast thermal animation | Normal |
| O | Eco / Balanced / High texture quality | High |
| A | Decorative pilot flames beneath dBFS meters | Off |
| W | Fourth screen Waterfall / stereo Fire | Waterfall |

The corresponding `SpectrumWall.ini` keys are `fire_palette=0..2`,
`fire_peaks=0..2`, `fire_lines=0|1`, `fire_speed=0..2`,
`fire_quality=0..2`, `fire_meters=0|1`, and
`fire_fourth=0|1`. `P` foreground particles, `E` engineering,
`J` center bias, and the main `V` visual-style cycle are unchanged.
The fourth Fire screen reuses the center layout's exact independent
L/R pane positions and measurement overlay, while its default
waterfall behavior remains unchanged.

## Aesthetic and target

A continuous, organic **audio furnace**, not a row of segmented LED bars:
a full-body flame bed with tapering tongues, yellow/white hot cores, dark
red edges and airborne particles. Flame height responds to the existing
spectrum envelopes; bass makes wider, slower surges, treble adds narrow,
fast flickers. Peak events create brighter tips and launch sparks. Do not
insert LED segment gaps in Fire mode.

Left portrait: left channel only. Right portrait: right channel only, with
the existing right portrait mirror toggle preserved. Center: symmetrical
left and right flames with bass bands facing inward when center mirroring
is enabled; keep the existing waveforms, level meters and stereo RMS bias
indicator fully functional. Fourth waterfall: existing waterfall initially
unmodified. Fire must be selectable without restarting optical capture.

## Proposed enum and state

1. Add `SpectrumStyle::Fire = 3` to `src/WallRenderer.hpp` and update
   `spectrumStyleName` and `cycleSpectrumStyle` from modulo 3 to modulo 4.
2. Extend CLI `--visual led|lines|hybrid|fire`, persist `visual=3` in
   `SpectrumWall.ini`, and validate range 0..3. Existing INI values 0..2
   remain compatible. `V` now cycles LED → Lines → Hybrid → Fire.
3. New `FireField` per spectrum pane: low-resolution heat buffer
   (suggested 25% of pane width and 33% of height), previous-frame heat,
   seeded turbulence, frame accumulator and optional last-peak information.
   Keep left and right buffers physically separate to avoid stereo bleed.
4. Optional later controls: `fire_tip=1` (initial default on), 
   `fire_hold=0`, `fire_lines=0`, `fire_palette=classic`. Do not add
   new keyboard shortcuts until the first hardware visual test demonstrates
   a need. Reuse `P` for particles, `G` for bloom, `E` for engineering.
5. Preserve `VisualState` as **read-only**. No change to audio capture,
   FFT bins, RMS, sample peak, calibration or bass pulse detection.

## Flame rendering data flow

- Each band `b` has envelope `e[b] = clamp(state.left/right[b],0,1)`.
  Use existing log-spaced bins with `visualBandIndex` / mirror mapping.
- Map bands to a low-res horizontal heat field, interpolating adjacent bins.
  Widen lower frequency hot kernels (e.g. 2.5 bins bass, 1 bin highs) to
  avoid comb-looking LED columns. This makes the bottom a continuous flame
  body even at 256 active bands.
- Inject heat in the low region of each flame pane according to
  `e[b]` and a per-band nonlinear flame-height mapping such as
  `height = pow(e[b],0.82)`. Silence must cool naturally rather than
  show animated fake fire; include a small ambient floor only when input
  exceeds a noise threshold.
- Upward advection: sample heat from previous-frame cells 1–3 vertical
  texels below, add deterministic horizontal turbulence and controlled
  diffusion, cool by height. Use semi-Lagrangian or equivalent bounded
  advection; clamp energy to 0..1 and avoid feedback runaway.
- Advance the heat field on a **fixed step** (~1/30 s) while drawing at
  the existing ~60 FPS. Cap catch-up to two simulation steps per frame.
  Screen resolution, frame rate and panel resize should not alter visual
  flame speed.
- Display heat using **Classic Fire** LUT (black → ember red →
  orange → gold → yellow → near-white). Composite with alpha or additively
  above the current dark grid background. Use SDL3 dynamic texture, scaled
  bilinear, or a mesh of gradients; do not require a heavyweight GPU
  compute API for the first pass.
- Distinct upward tongues use local turbulence and time-varying tip
  segmentation, but NO rectangular LED break lines.
- Bass pulse modulates lower flame thickness, rise speed and transient
  thermal injection—not meter calibration. Mid/high band updates drive
  delicate tips. Make pulsing bounded to avoid a static red wall at
  loud levels.

## Peaks and sparks

Initial Fire mode has **no conventional peak-hold line** and **no
additional waveform over flame body**. A brief hot tip and sparks are
optional visualization embellishments, with peak hold / fine line overlays
explicitly deferred until the user sees the first hardware output.

The existing common `drawParticles` pass must remain available via **P**
in every visualization mode. The visible-sparks change in this branch
moves particles to a **foreground** pass: launch near FFT-height tips,
rise/fade with a bounded pool, and use per-channel mirror coordinates.
For Fire, tint the particles yellow/orange/white rather than cycling all
palettes (optional fire-specific palette adaptation). The size and count
must be constrained for the RX 580 and all three monitors.

## Rendering order (each screen)

1. Clear / dark grid.
2. Optional GPU glow backing.
3. Active visual style: LED, Lines, Hybrid, **or Fire** (mutually exclusive).
4. Audio sparks (P toggle), optional existing 3D objects on center.
5. Engineering axes, frequency buckets and dBFS hover probes (E).
6. Stereo RMS bias gauge (J, center only).
7. Status HUD and F1/right-click shortcut popup.

The engineering overlay **must be drawn after Fire on ALL screens**:
center mirrored frequency scales, portrait frequency and dBFS axes,
cursor probes, bottom calibrated peak / RMS bars, and the center
stereo balance indicator. The Fire visual must not modify the underlying
dBFS values or frequency-to-pixel math.

## Measurable performance/safety targets

- Maintain smooth rendering at 60 FPS with 3 displays on RX 580-class
  hardware. Dynamic heat texture resolution should be capped by screen
  area, not number of FFT bands. Avoid per-frame heap reallocations.
- Stable 12-hour+ unattended run on live Realtek optical S/PDIF with
  A2FPGA/IIgs MOD playback. No increasing textures/particle vectors.
- Mono L-only signal should not light up right channel flame or vice
  versa (except background grid). Under silence flames fade completely.
- Killed/null audio source and window resizing must not crash or cause
  bright/full-scale visual artifacts.
- At 64, 96, 128, 192 and 256 bands: correct edge bins and mirrored
  center orientation with no array overruns.
- Engineering E overlays, hover probes, J balance gauge, P particle
  toggle, F1 shortcut popup, K/U/I local Retro3D, and INI round-trip
  remain operational on all supported displays.

## Suggested implementation milestones

**F1 — Particle release in main (this branch):** Make sparks genuinely
visible in LED/Lines/Hybrid, emitted near peaks rather than hidden behind
bars, add explicit HUD status and test their mirror/silence/decay helpers.
Merge only after Windows CI succeeds. This is independently shippable.

**F2 — First Fire visual:** Add the fourth style plus heat field; classic
red/orange/yellow flames, organic tongues, stereo mirroring, and existing
particles. No peak-hold line. Publish a test ZIP.

**F3 — User hardware calibration:** Tune flame width, rise/fall time,
brightness, silence floor, bass impulse coupling, spark density and
engineering-axis readability against the live optical IIgs MOD stream.
Collect FPS and GPU load across 3 screens and run an overnight soak.

**F4 — Implemented optional refinements:** Peak flash/hold sprites,
thin ridge overlay, Classic/Blue Gas/Plasma palettes, Slow/Normal/Fast
animation speed, Eco/Balanced/High thermal texture quality, optional
decorative pilot-flame meters with calibrated dBFS instruments intact,
and optional fourth-screen Fire view with engineering overlays.

**Next hardware validation:** compare palette colors, held/flash tips,
line and pilot-meter toggles, speed presets, quality/FPS tradeoffs,
fourth-screen enable/disable, and simultaneous particles + engineering
on live IIgs/a2fpga optical MOD audio. Fire options default to the
original first-pass appearance for safe A/B testing.
