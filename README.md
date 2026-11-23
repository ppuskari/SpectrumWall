# SpectrumWall

**SpectrumWall** is a Windows 11 multi-monitor live audio visualizer: independent left/right portrait LED spectrums, a central stereo visualizer and an optional fourth waterfall display. It is built for an AMD RX 580-class GPU, but uses standard SDL3 rendering.

## Audio source priority

The preferred source is the **Realtek optical S/PDIF recording endpoint**. The intended signal path is:

`a2fpga HDMI -> HDMI audio extractor -> TOSLINK -> Realtek S/PDIF IN -> Windows WASAPI -> SpectrumWall`

SpectrumWall looks for Realtek / Digital Input / S/PDIF recording-device names on startup. It does **not** capture the Realtek Digital Output endpoint by mistake. It can also use other recording inputs or WASAPI playback loopback.

## First build (Windows 11)

Requirements: Visual Studio 2022 with C++ Desktop workload, CMake 3.24+, Git, vcpkg and internet access for the miniaudio dependency.

```powershell
git clone https://github.com/ppuskari/SpectrumWall
cd SpectrumWall
$env:VCPKG_ROOT = "C:\src\vcpkg"  # adapt to your installation
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release
.\build\Release\SpectrumWall.exe --list-devices
.\build\Release\SpectrumWall.exe
```

Depending on your CMake/vcpkg generator, the executable may be under `build\src\Release\` or `build\Release\`.

### Command-line options

- `--list-devices`: enumerate Windows capture and WASAPI loopback devices.
- `--source "Digital Input"`: prefer a matching device name (case-insensitive).
- `--loopback`: start on the default Windows playback loopback device instead of S/PDIF input.
- `--demo`: use synthetic stereo audio for visual testing.
- `--fourth`: enable the optional second horizontal display if one exists.
- `--windowed`: open movable bordered windows rather than borderless screen-filling displays.
- `--visual led|lines|hybrid`: choose LED segments, smooth curves, or a layered combination (default `lines`).
- `--engineering`: enable engineering dBFS/frequency-axis legends and exact FFT bucket mouse probes at startup.
- `--stereo-balance`: enable the independent center-monitor L/R RMS bias meter at startup.
- `--palette N`: choose color scheme **1–8** (default 4, Prism Rainbow).
- `--smoothing fast|balanced|liquid`: choose attack/release response (default balanced).

### Persistent settings (SpectrumWall.ini)

SpectrumWall now restores the previous live display arrangement automatically.
**On normal exit** (Esc, Q, or closing a window), it writes exactly one small
configuration update to `SpectrumWall.ini` **beside the executable**. On next
launch it reads that file before processing command-line arguments, so flags
you supply explicitly always override saved values.

The file is a plain UTF-8-ish `key=value` text file and can be edited while
SpectrumWall is closed. It includes band count, selected palette, LED/Lines/
Hybrid style, smoothing profile, frequency softening, GPU glow intensity,
afterglow, reflections, particles, center/right portrait mirroring,
Engineering Mode, stereo RMS bias meter, fullscreen/windowed and optional
fourth display selection, demo/loopback status, and the preferred WASAPI
capture endpoint name. The exact audio endpoint is selected by name rather
than transient numeric device index; if it disappears, automatic detection
falls back to the Realtek optical input when available.

For example, after closing a session in Hybrid / 128 bands with axes and
balance shown, the file contains values such as:

```ini
version=1
bands=128
palette=4
visual=2
smoothing=1
engineering=1
stereo_balance=1
glow=2
center_mirror=1
right_portrait_mirror=1
source=Realtek Digital Input (Realtek High Definition Audio)
```

Useful startup overrides:

```powershell
.\\SpectrumWall.exe
.\\SpectrumWall.exe --visual hybrid --bands 192
.\\SpectrumWall.exe --fullscreen --no-engineering
.\\SpectrumWall.exe --defaults
.\\SpectrumWall.exe --no-save --windowed
```

`--defaults` ignores the existing INI for that session and saves the new
settings when you exit. `--no-save` tests a temporary arrangement without
changing the stored preferences. The save writes a temporary file and then
replaces the INI; interrupted writes should leave the prior INI intact. If the
folder isn't writable (e.g. Program Files), SpectrumWall warns in the
console and you should move the portable executable to a writable directory.

### Live spectrum density

The default is **128 bands**. You can choose any integer from 64 to 256 when launching:

```powershell
.\\SpectrumWall.exe --bands 128
.\\SpectrumWall.exe --bands 256 --windowed
```

Press **[** and **]** to change the count in increments of 16, or **B** to cycle through 64, 96, 128, 192 and 256. All active screens update together without restarting Realtek capture. Each fullscreen view shows a small `N BANDS` indicator.

### Shortcut help overlay

**Right-click anywhere on a SpectrumWall monitor** to open a keyboard
shortcut reference on that specific display. **F1** does the same on the
focused window. The overlay floats above LED, Fluid Lines, Hybrid and
Engineering Mode without interrupting the live stereo capture.

At normal center or portrait resolutions, every shortcut appears in two
readable columns. Small windowed previews show the same reference in two
pages: press **Tab** or roll the mouse wheel to switch pages.

To dismiss it, **left-click**, right-click again on that display, press
**F1**, or press **Esc**. While help is open, **Esc only closes the help**;
press it a second time to exit SpectrumWall. **Q always exits**, saving
`SpectrumWall.ini` as usual. The open/closed help state is intentionally
not saved as part of your display preferences.

### Hotkeys

- **Esc** — close shortcut help if open; otherwise exit
- **Q** — exit and save settings
- **Right-click / F1** — toggle the shortcut reference on a display
- **Left-click** — dismiss shortcut help when open
- **Tab / mouse wheel** — switch shortcut pages on compact windows
- **N** — next capture or loopback device in the current category
- **L** — toggle between capture sources and playback loopbacks
- **D** — toggle the synthetic audio test pattern
- **1–8** — Classic Hi-Fi, Neon Nights, Golden Amber, Prism Rainbow, Aurora, Inferno, Ice Blue or Vintage Phosphor
- **C** — cycle all eight color schemes
- **S** — cycle Fast / Balanced / Liquid temporal smoothing
- **F** — toggle light frequency-neighbor smoothing (visual only)
- **G** — cycle GPU glow Off / Soft / Vivid
- **T** — toggle fading LED afterglow trails
- **R** — toggle reflected light below the frequency bars
- **P** — toggle bass-reactive drifting particles
- **M** — toggle center-monitor stereo mirror (default on: bass from both channels meets at the middle)
- **H** — toggle right portrait monitor frequency mirroring (default on; left portrait unchanged)
- **V** — cycle LED → Fluid Lines → Hybrid visual styles without restarting capture
- **E** — toggle Engineering Mode axes and live frequency/dBFS bucket probes
- **J** — toggle the center stereo balance gauge (separate from Engineering Mode)
- **[ / ]** — decrease / increase active spectrum bars by 16, clamped to 64–256
- **B** — cycle through 64, 96, 128, 192 and 256-band presets

The application prints available audio endpoints and the currently selected source to the console. An eventual in-app control panel and explicit per-monitor assignment remain future enhancements; display preferences are now persisted automatically.

## Center-monitor stereo symmetry

By default, the **left half of the center screen is mirrored horizontally**.
The left channel is arranged high-to-low from the outside toward the center;
the right channel remains low-to-high from the center toward the outside:

`TREBLE ← LEFT CHANNEL ← BASS | BASS → RIGHT CHANNEL → TREBLE`

Both low-frequency sections meet at the center divide. The left portrait display retains
its original low-to-high frequency orientation; the right portrait now mirrors its
frequency axis to high-to-low by default, matching the opposite screen's symmetrical look.
This is **rendering-only**:
FFT bins, stereo levels, waveforms, peak tracking, colors and optical recording remain
unchanged. The left half's glow, reflections and animated peak indicators are mirrored
with the bars, keeping all effects spatially aligned.
The lower-left center **VU level meter, peak marker and -2 dB target line**
now mirror with the upper-left spectrum. The center-right VU meter stays normal.

Press **M** to switch between mirrored and original center layouts live.
Press **H** to independently toggle the right portrait's mirroring. Its bars, peak hold,
frequency gradients, reflections, and GPU bloom all follow the mirrored X positions.
The left portrait, center stereo panels, waveforms, VU meters, and audio capture
remain unchanged by **H**.

## Engineering Mode and stereo balance instrumentation

Both instrumentation layers are **optional and independently switchable**. All
LED/Fluid Lines/Hybrid palettes and the left/center/right mirror mappings work
normally with either layer enabled.

- Press **E** for Engineering Mode. The center graph gains a vertically
  calibrated dBFS axis **up its middle** and frequency bucket scales below
  **both** mirrored halves. The left and right portrait displays show the same
  decibel and logarithmic frequency axes, correctly respecting the right
  portrait mirror toggle.
- The displayed spectral axis spans **-72 to -2 dBFS**, matching the current
  normalized visual envelope. Its values refer to the visual range, **not** a
  calibration of the physical audio input. Tick labels identify the plotted
  **30 Hz–18 kHz** logarithmic spectrum.
- The engineering frequency tick labels, dBFS numbers, probe text and balance
  gauge readout are rendered **30% larger** than the original tiny SDL3 font.
  Tick spacing increases to keep mirrored portrait legends readable.
- Hover your mouse over a channel spectrum to see the nearest **actual FFT
  bucket number, frequency range in Hz and raw per-band peak dBFS reading**.
  The probe uses the pre-smoothed Hann-windowed FFT magnitude, *not* the
  temporally smoothed display height. Due to the current 4096-sample FFT,
  adjacent display buckets can share FFT bins, particularly at low frequencies.
- In Engineering Mode, **both lower center level meters** now have their
  own **-72 to 0 dBFS rulers**, with the left-hand scale mirrored together with
  its VU bar. The small cyan indicator shows the *current unsmoothed full-band
  RMS*, the bright white mark shows the *current sample peak*, and the filled
  level bar remains a *smoothed RMS* for visual stability. All positions use
  the **same logarithmic dBFS scale** (older releases incorrectly placed the
  peak marker by raw linear amplitude).
- Just below each meter, an always-visible readout prints **RMS dBFS** and
  **SAMPLE PK dBFS** for that channel. Hover the mouse over either bottom meter
  to inspect the **cursor's dBFS threshold** alongside the live RMS/peak values.
  The pointer and scale honor the center-left mirror toggle, and the -2 dBFS
  headroom reference is now placed accurately on the logarithmic scale.
- Audacity's recording/playback meters combine a current peak, RMS overlay, and
  recent peak hold. Our RMS bars can legitimately look much lower than
  Audacity's peak level (e.g., -12 dBFS RMS with a -2 dBFS sample peak).
  For a meaningful check, compare **SpectrumWall SAMPLE PK dBFS** with
  Audacity's **current peak**, using the same physical input and comparable
  time windows. The current release uses a 4096-frame (~85 ms at 48 kHz)
  analysis window, so fast transient timing can still differ. The lower meter
  is a sample-peak display, not an intersample true-peak meter.
- Press **J** to show the **STEREO RMS BIAS** gauge beneath the center
  waveforms. The pointer slides left for left-channel dominance and right for
  right-channel dominance, in **dB of overall R/L RMS amplitude ratio**.
  Zero means both channels carry equal overall RMS energy. Its calibrated
  visual range is **-18 to +18 dB** with graduated ±6 and ±12 dB ticks;
  readings outside that scale peg the marker but remain visible numerically.
  The numeric readout also shows each channel's full-band RMS in dBFS. The
  display pointer has responsive smoothing, while the number reports the
  current RMS balance. A silence gate prevents noise from producing wild
  bias swings. This is a *level-balance* indicator, not stereo phase correlation
  or stereo width measurement.

Both flags are opt-in and disabled by default:

```powershell
.\\SpectrumWall.exe --visual hybrid --bands 128 --engineering --stereo-balance
```

You can activate or deactivate both features live via E and J, without
reinitializing optical S/PDIF capture.

## Fluid Lines / Peak Traces

The default `--visual lines` display replaces the LED stacks with flowing spectral
curves. The graph has three independently drawn envelopes:

- **Live**: brightly colored, continuously interpolated line corresponding to
  the current stereo FFT, with an energy-colored translucent fill below it.
- **Peak hold**: pale, thin, lingering line above the live signal, representing
  the existing per-frequency peak-hold state.
- **Afterglow**: the former live envelope decays into a faint trailing curve
  that makes transients and fades easier to follow.

The Hermite interpolation stays between neighboring FFT values to avoid artificial
overshoot. It smooths the *geometry* between displayed bands rather than claiming
more FFT frequency resolution. The same channel-independent data and 64–256 band
selection are retained. Vertical grids and gentle neon outlines evoke a classic
oscilloscope and the retro-futuristic performance monitors of Task Manager OG
(TMOG visuals are an aesthetic reference only; no external application code is used).

`--visual hybrid` overlays the live and peak curves on the existing LED columns,
while `--visual led` preserves the previous look. Press **V** at any time to cycle
through them. Existing palettes (1–8), glow (G), smoothing (S), frequency softening
(F), trails (T) and stereo mirror toggles (M, H) remain available.

Example commands:

```powershell
.\\SpectrumWall.exe --bands 128 --palette 4 --visual lines
.\\SpectrumWall.exe --bands 128 --palette 5 --visual hybrid --smoothing liquid
.\\SpectrumWall.exe --visual led
```

## Eye-candy release

This edition adds two independent types of smoothing: temporal attack/release profiles
(Fast: punchy, Balanced: responsive, Liquid: flowing) and optional display-domain
three-tap frequency smoothing. Neither alters the optical TOSLINK input or stereo RMS/peak
measurements. A longer FFT is a separate future feature; smoothing does not
create frequency resolution not present in the 4096-point FFT.

The spectrum gains dynamic frequency-and-height-sensitive HSV color gradients,
36 LED segments per band, bright peak glints, afterglow decay, reflected LEDs,
color-matched scrolling waterfall, and an optional multi-pass GPU bloom layer
using one low-resolution render-target texture per monitor (SDL3 additive compositing).
If a graphics renderer cannot create the bloom target, the LED rendering continues
without that pass. Effect features can be toggled live. The console reports
the actual rendered FPS for multi-monitor RX 580 performance assessment.

Example launch on your a2fpga Realtek optical input:

```powershell
.\\SpectrumWall.exe --bands 128 --palette 4 --smoothing balanced
.\\SpectrumWall.exe --bands 192 --palette 5 --smoothing liquid --windowed
```

**The Windows ZIP is a portable executable and SDL3.dll, not an installer.**
Extract them together. In fullscreen, press Esc or Q to close.

## Architecture

A single stereo capture path feeds a rolling PCM ring buffer. FFT, peaks and RMS are computed once, and the resulting visualization state is shared across separate SDL3 renderer windows. The current implementation targets 48 kHz float stereo conversion, a 4096-point Hann FFT, 64–256 log-spaced bands (default 128), peak hold and a 60 FPS render loop. FFT band boundaries are cached on resolution changes. Frequency bin resolution remains approximately 11.72 Hz at 48 kHz, so additional bars do not add FFT frequency discrimination.

## Known constraints of the first proof of concept

- No hardware is needed to test `--demo`.
- The underlying Realtek recording endpoint must appear and be enabled in Windows Sound settings. S/PDIF **output** does not serve as an input.
- SDL3 uses the OS-selected GPU renderer (often D3D11/12 on Windows). Native GLSL shaders and a graphical device chooser are future enhancements; current glow uses SDL3 GPU render targets, and LED afterglow is implemented.
- Rendering on all connected displays is not yet performance-benchmarked or tested on the target multi-monitor RX 580 system.
- The first program uses a small mutex-protected PCM ring; it should eventually be replaced by a lock-free audio transport for stronger hard real-time behavior.

## License

MIT. See LICENSE.
