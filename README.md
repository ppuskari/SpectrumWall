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

### Live spectrum density

The default is **128 bands**. You can choose any integer from 64 to 256 when launching:

```powershell
.\\SpectrumWall.exe --bands 128
.\\SpectrumWall.exe --bands 256 --windowed
```

Press **[** and **]** to change the count in increments of 16, or **B** to cycle through 64, 96, 128, 192 and 256. All active screens update together without restarting Realtek capture. Each fullscreen view shows a small `N BANDS` indicator.

### Hotkeys

- **Esc** or **Q** — exit
- **N** — next capture or loopback device in the current category
- **L** — toggle between capture sources and playback loopbacks
- **D** — toggle the synthetic audio test pattern
- **1 / 2 / 3** — switch between Classic, Neon and Amber LED palettes
- **[ / ]** — decrease / increase active spectrum bars by 16, clamped to 64–256
- **B** — cycle through 64, 96, 128, 192 and 256-band presets

The application prints available audio endpoints and the currently selected source to the console. An eventual in-app control panel and persistent monitor mapping are tracked for future releases.

## Architecture

A single stereo capture path feeds a rolling PCM ring buffer. FFT, peaks and RMS are computed once, and the resulting visualization state is shared across separate SDL3 renderer windows. The current implementation targets 48 kHz float stereo conversion, a 4096-point Hann FFT, 64–256 log-spaced bands (default 128), peak hold and a 60 FPS render loop. FFT band boundaries are cached on resolution changes. Frequency bin resolution remains approximately 11.72 Hz at 48 kHz, so additional bars do not add FFT frequency discrimination.

## Known constraints of the first proof of concept

- No hardware is needed to test `--demo`.
- The underlying Realtek recording endpoint must appear and be enabled in Windows Sound settings. S/PDIF **output** does not serve as an input.
- SDL3 uses the OS-selected GPU renderer (often D3D11/12 on Windows). Shader bloom, persistent trails and a graphical device chooser are future enhancements.
- Rendering on all connected displays is not yet performance-benchmarked or tested on the target multi-monitor RX 580 system.
- The first program uses a small mutex-protected PCM ring; it should eventually be replaced by a lock-free audio transport for stronger hard real-time behavior.

## License

MIT. See LICENSE.
