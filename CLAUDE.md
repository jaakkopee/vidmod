# CLAUDE.md

Audio-reactive video effect processor (C++17, SFML/TGUI GUI). See README.md for
features, per-effect algorithm descriptions, and usage; EXAMPLES.md for effect
recipes. This file covers only what the docs don't, plus places where the docs
are wrong.

## Build

```bash
./build.sh          # cmake + make into build/, binary at build/bin/VidMod
```

Dependencies per CMakeLists.txt: **SFML 3** and **TGUI 1** (`find_package`),
FFTW3 / OpenCV4 / libsndfile (via pkg-config), nlohmann_json ≥ 3.2, OpenMP
(optional; hardcoded Homebrew libomp paths on macOS). `ffmpeg` is a **runtime**
dependency — audio muxing shells out to it (`VideoProcessor.cpp`, `GUI.cpp`).

⚠️ README says SFML 2.5+ / TGUI 0.9+ — outdated. The code uses the SFML 3 API
(`sf::VideoMode({w, h})`, `event->is<...>()`) and will not compile against SFML 2.

## Adding a new effect

There is **no central registry**. Effects are wired by exact string match on the
name passed to the `Effect` base constructor, in three hand-maintained lists that
must all agree:

1. **Header + cpp**: subclass `Effect` (include/Effect.h). The constructor calls
   `Effect("Name")` and `setParameter(...)` for every parameter with its default.
   Implement:
   - `apply(frame, audioBuffer, videoFps)` — must handle `audioBuffer == nullptr`
     (convention: `return frame.clone();`)
   - `getParameterNames()` — order here is the GUI parameter-panel order
   - optionally `getParameterNominalMax(name)` — canonical max used for
     automation range scaling; override when a param's natural max isn't
     `max(1, |value|)` (e.g. color biases → 255)
2. **CMakeLists.txt**: add the cpp to `SOURCES`.
3. **GUI.cpp** (two spots): `effectList->addItem("Name")` in `setupUI`
   (~line 589) so it shows in "Available Effects", and an `else if` branch in
   `GUI::addEffectToChain` (~line 1056). Add the `#include` at the top of
   GUI.cpp (GUI.h only includes the older effects).
4. **EffectChain.cpp**: `#include` + an `else if` branch in
   `EffectChain::fromJsonString` (~line 197). **Skipping this silently breaks
   preset loading** — unknown names are logged and dropped, not errored.

Constructor defaults double as backwards compatibility: presets saved before a
parameter existed load fine because missing keys keep the default. Don't rename
or repurpose an existing parameter key.

## Per-frame audio-sync invariant

`EffectChain::applyEffects` (src/EffectChain.cpp:49) enforces: **every effect in
a frame reads the same audio slice, and the cursor advances exactly once per
frame.** Mechanics:

- `AudioBuffer::getBuffer(n)` **advances the index as a side effect** (it calls
  `getNext()` n times, wrapping circularly — this is also what makes audio loop).
- So the chain saves `audioBuffer->getIndex()` before the loop, calls
  `setIndex(saved)` before **each** effect, and after the loop sets
  `setIndex(saved + sampleRate / videoFps)`.
- Effects compute their slice size themselves as
  `static_cast<int>(audioBuffer->getSampleRate() / videoFps)`.

Any change touching audio indexing (in effects, AudioBuffer/CircularBuffer, or
the chain) must preserve this. An effect that reads more/fewer samples than
`sampleRate/fps` is fine — the restore/advance in the chain corrects for it.

Note: the `Rhythmo*` effects are **stateful across frames** — each instance owns
a `RhythmogramDSP` (shared filterbank front-end, src/RhythmogramDSP.cpp),
recreated only when the sample rate changes. They assume audio arrives in
forward frame order.

## Parameter typing footgun

`Effect::parameters` is `std::map<std::string, float>` — but several params are
integer-like and get `static_cast<int>` (truncation) at use: `iterations`,
`kernel_size`, `morph_iterations`, `kernel_growth`, `mode` (AudioColor). CAGlow
is the exception and uses `std::lround`. Automation and JSON round-trips can
therefore produce off-by-one steps (e.g. 2.999 → 2). JSON load accepts numbers
and booleans (bool → 0/1); other value types are silently ignored.

## Preset JSON

Two on-disk shapes, both accepted by `fromJsonString`:
- legacy: `{ "version", "effects": [{ "name", "parameters" }] }` at top level
- current: `{ "effectChain": {legacy shape}, "automation": {...} }` (written by
  the GUI when the automation toggle is on; see GUI.cpp ~line 1772)

A failed load leaves the current chain intact; individual unknown effects are
skipped, not fatal.

## Memory / processing model

- **Audio**: fully decoded into RAM as mono float (`VideoProcessor::loadAudio`).
- **Video**: streamed, not loaded into RAM. `VideoProcessor` reads frame-by-frame
  from `cv::VideoCapture`; the GUI's threaded render path uses `VideoBuffer`,
  which buffers 100 frames at a time from disk and loops automatically.
  ⚠️ README's "loads entire videos into memory" note is outdated.
- Output is written with OpenCV `mp4v`, then re-encoded to libx264 + AAC by the
  ffmpeg mux step when audio is present.

## Testing

No automated test suite. Generate test inputs with:

```bash
python3 generate_test_media.py                                    # 3 images + 3×180s videos
python3 generate_test_media.py --duration 12 --fps 24 --video-size 960x540   # fast smoke test
```

(needs `pip install numpy opencv-python`), then verify manually through the GUI
(load video/audio → add effects → Preview → Process).

## Open questions

- Whether current Homebrew/apt packages of SFML/TGUI satisfy the SFML 3 / TGUI 1
  requirement — the README install commands predate the version bump and are
  unverified.
- `fftvidmod2.py` is the original Python prototype (per README credits); its
  current relationship to the C++ code is unverified — treat as reference only.
