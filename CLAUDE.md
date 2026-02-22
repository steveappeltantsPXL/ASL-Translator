# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

Real-time bidirectional ASL ↔ English translation — native C++20 desktop app (Windows primary).  
The app is currently a **UI shell only**: SDL3 window + ImGui layout + OpenCV/ONNX Runtime linked but not yet wired up.  
See `docs/00-PROJECT-STATUS.md` for what is and isn't built.

## Build

**Always use the Visual Studio 17 2022 generator on Windows. Never Ninja** (Ninja + MSVC defaults to x86; SDL3 and ONNX Runtime are 64-bit only).

CLion build directory is `cmake-build-debug-msvc/`. The command CLion runs:
```powershell
cmake --build cmake-build-debug-msvc --config Debug
```

Manual configure + build from project root:
```powershell
cmake -B cmake-build-debug-msvc -G "Visual Studio 17 2022" `
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build cmake-build-debug-msvc --config Debug
.\cmake-build-debug-msvc\Debug\VisearASLTranslator.exe
```

Clean rebuild: delete `cmake-build-debug-msvc/` and re-run configure.

**Tests:** GTest is installed but no tests exist yet. Uncomment the GTest block in `CMakeLists.txt` and create `tests/CMakeLists.txt` first.

## Code style

Enforced by `.clang-format` (Google base, C++20):
- 4-space indent, 100-column limit
- `int*` / `int&` pointer/reference alignment (left)
- Include order: project headers → SDL3/ImGui vendor → system/STL

## Architecture

Three conceptual layers (none fully implemented yet):

**Input** → camera (`opencv_videoio`), microphone (SDL audio / PortAudio), text input (ImGui)  
**Processing** → MediaPipe landmarks → ONNX gesture classifier → sentence assembly → whisper.cpp STT / Piper TTS  
**Output** → ImGui captions, virtual camera (OBS VCam), virtual microphone (VB-Audio)

Six translation pipelines: ASL→Text, ASL→Speech, Speech→Text, Speech→ASL, Text→ASL, Text→Speech.

### Current source layout

```
src/
└── main.cpp          — entire app today: SDL3 init, ImGui render loop, UI layout
src/app.rc            — Windows resource: embeds app_icon.ico into the .exe
src/assets/
└── app_icon.ico      — multi-resolution ICO (16/32/48/256/512 px, PNG-in-ICO)
vendor/imgui/         — Dear ImGui docking branch (git submodule)
tools/
└── make_ico.ps1      — converts a PNG (even one named .ico) to a real ICO file
```

Planned structure (see `docs/07-APPLICATION-GUIDE.md`):
```
src/app/              — Application class, config system
src/ui/               — UIManager, panel classes
src/capture/          — CameraCapture (OpenCV RAII wrapper)
src/ml/               — ONNXRuntime session, GestureClassifier
src/pipeline/         — TranslationPipeline (orchestrates all 6 flows)
resources/models/     — .onnx files + model_registry.json
tests/                — GTest unit + integration tests
training/             — Python/PyTorch training pipeline (separate from C++ app)
```

### ImGui render loop

Every frame in `main.cpp`:
```
SDL_PollEvent → ImGui_ImplSDL3_ProcessEvent
ImGui_ImplOpenGL3_NewFrame / ImGui_ImplSDL3_NewFrame / ImGui::NewFrame
[build UI]
ImGui::Render → glClear → ImGui_ImplOpenGL3_RenderDrawData → SDL_GL_SwapWindow
```

### Desktop layout (width ≥ 640 px)

```
y=0,  h=40   ##Toolbar (NoTitleBar)  ► Start Capture / ■ Stop / ● Running / fps
y=40, h=top  Camera Feed (35%) | ASL Avatar (40%) | Controls (25%, full height)
             ─────────────────────────────────────┘
y=bot, h=80  Captions (75% width, Controls overlaps bottom-right)
```

`top_panels_height = DisplaySize.y − 40 − 80`. Controls extends to `DisplaySize.y − 40`.
Mobile mode (< 640 px): Avatar fills top, Captions strip 60 px at bottom.

### Fonts

Calibri 16 px (primary, OversampleH=3, PixelSnapH=true) merged with Segoe UI 16 px for symbol ranges `0x2190–0x23FF` and `0x25A0–0x26FF`. Static glyph range arrays must remain `static` — ImGui holds a pointer across frames. Do not add or resize fonts after `ImGui_ImplOpenGL3_Init`.

### Window icon

Embedded via `src/app.rc` — no runtime loading needed. When replacing the icon:
1. Drop a PNG named `app_icon.ico` into `src/assets/`
2. Run `powershell -ExecutionPolicy Bypass -File tools\make_ico.ps1` from project root
3. Rebuild — the RC compiler embeds the new ICO automatically

## Critical CMake / vcpkg rules

- `vcpkg.json` `builtin-baseline` must be a 40-char commit SHA, not a date. Get it with `git -C C:\vcpkg rev-parse HEAD`.
- `onnxruntime-gpu` ships no CMake config. Use `find_path`/`find_library` as already done in `CMakeLists.txt` — never `find_package(onnxruntime CONFIG REQUIRED)`.
- Adding a vcpkg dependency: add to `vcpkg.json` **and** add `find_package` + `target_link_libraries` in `CMakeLists.txt`.
- ImGui is a vendored static lib (`vendor/imgui/`, docking branch). Never add it to vcpkg.
- `whisper.cpp` and `piper` are commented out in `CMakeLists.txt` — uncomment only after adding their git submodules.

## Windows-specific rules

- `<GL/gl.h>` requires `<windows.h>` before it on MSVC — always keep the `#ifdef _WIN32` guard.
- `/utf-8` compile flag is mandatory. Without it MSVC corrupts non-ASCII string literals (► ● ⚙ render as `?`).
- Supplementary-plane emoji (U+1F000+) cannot render in ImGui — use BMP Unicode symbols (≤ U+FFFF) or `ImDrawList` primitives.
- The accent color (toolbar button tint) is read at startup via `DwmGetColorizationColor` into `ImVec4 accent_color`. `dwmapi` is linked in `CMakeLists.txt`.
- Runtime library is `MultiThreadedDLL` (`/MD`) — must match vcpkg triplet `x64-windows`.

## ML pipeline (not yet implemented)

Models will live in `resources/models/` as ONNX files. Input shape for the gesture classifier: `[1, 60, 225]` (batch, frames, landmarks).  
GPU execution: DirectML on Windows, CoreML on macOS, CUDA on Linux — use `#ifdef VISEAR_PLATFORM_*` guards.  
See `docs/06-ML-PIPELINE.md` for the full training → export → inference workflow.