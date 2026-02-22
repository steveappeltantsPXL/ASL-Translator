# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

Real-time bidirectional ASL ↔ English translation — native C++20 desktop app (Windows primary).
The app has a **responsive ImGui UI shell** with a **live Tier 2 rigged 3D avatar renderer** (OpenGL FBO + GLTF skinned mesh). Camera capture, MediaPipe, ONNX inference, and speech pipelines are not yet wired up.
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
**Required after any `find_package` or linking change in CMakeLists.txt.**

**Tests:** GTest is installed but no tests exist yet. Uncomment the GTest block in `CMakeLists.txt` and create `tests/CMakeLists.txt` first.

## Code style

Enforced by `.clang-format` (Google base, C++20):
- 4-space indent, 100-column limit
- `int*` / `int&` pointer/reference alignment (left)
- Include order: project headers → SDL3/ImGui vendor → system/STL

## Architecture

Three conceptual layers (avatar output layer is implemented; others pending):

**Input** → camera (`opencv_videoio`), microphone (SDL audio / PortAudio), text input (ImGui)
**Processing** → MediaPipe landmarks → ONNX gesture classifier → sentence assembly → whisper.cpp STT / Piper TTS
**Output** → ImGui captions, **Tier 2 rigged 3D avatar** (implemented), virtual camera (OBS VCam), virtual microphone (VB-Audio)

Six translation pipelines: ASL→Text, ASL→Speech, Speech→Text, Speech→ASL, Text→ASL, Text→Speech.

### Current source layout

```
src/
├── main.cpp              — SDL3 init, ImGui render loop, UI layout, AvatarRenderer integration
├── app.rc                — Windows resource: embeds app_icon.ico into the .exe
├── assets/
│   └── app_icon.ico      — multi-resolution ICO (16/32/48/256/512 px)
└── avatar/               — Tier 2 rigged 3D avatar (complete)
    ├── AvatarRenderer.h/.cpp   — pImpl façade: GLEW init, FBO, per-frame render, setPose()
    ├── AvatarShaders.h         — GLSL 3.30 skinned-mesh vertex + Phong fragment (inline)
    ├── GltfLoader.h/.cpp       — tinygltf GLB loader: mesh, skeleton, animations
    ├── SkinnedMesh.h/.cpp      — VAO/VBO/IBO, glVertexAttribIPointer for bone IDs
    ├── Skeleton.h/.cpp         — joint hierarchy, computeSkinMatrices()
    └── AnimationPlayer.h/.cpp  — keyframe sampler (linear T/S, slerp R), idle loop

vendor/
├── imgui/                — Dear ImGui docking branch (git submodule)
└── tinygltf/             — vendored headers: tiny_gltf.h, stb_image.h (NOT a submodule)

tools/
└── make_ico.ps1          — converts a PNG to a proper multi-resolution ICO

resources/
└── models/
    └── avatar/           — place avatar.glb here (Mixamo → Blender GLTF export)
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
avatarRenderer.render(panelW, panelH, io.DeltaTime)   ← FBO render BEFORE ImGui frame
ImGui_ImplOpenGL3_NewFrame / ImGui_ImplSDL3_NewFrame / ImGui::NewFrame
[build UI — avatar panel calls ImGui::Image(avatarRenderer.getTexture(), avail, {0,1}, {1,0})]
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

### OpenGL context

**GL 3.3 core** — required for GLSL 3.30 (used by avatar shaders) and `glVertexAttribIPointer` (bone ID integer attributes). GLSL version string: `"#version 330 core"`.

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
- `vendor/tinygltf/` is committed directly (not a submodule). Contains `tiny_gltf.h` and `stb_image.h`.
- `whisper.cpp` and `piper` are commented out in `CMakeLists.txt` — uncomment only after adding their git submodules.
- After any `find_package` or linking change, delete `cmake-build-debug-msvc/` and reconfigure.

## Windows-specific rules

- `<GL/gl.h>` requires `<windows.h>` before it on MSVC — always keep the `#ifdef _WIN32` guard.
- `/utf-8` compile flag is mandatory. Without it MSVC corrupts non-ASCII string literals (► ● ⚙ render as `?`).
- Supplementary-plane emoji (U+1F000+) cannot render in ImGui — use BMP Unicode symbols (≤ U+FFFF) or `ImDrawList` primitives.
- The accent color (toolbar button tint) is read at startup via `DwmGetColorizationColor` into `ImVec4 accent_color`. `dwmapi` is linked in `CMakeLists.txt`.
- Runtime library is `MultiThreadedDLL` (`/MD`) — must match vcpkg triplet `x64-windows`.
- `ImTextureID` is `ImU64` in this ImGui build — use `static_cast<ImTextureID>(texId)`, never `reinterpret_cast` or `(void*)`.

## GL loader isolation (avatar system)

`imgui_impl_opengl3.cpp` includes its own `imgui_impl_opengl3_loader.h` which defines GL functions and macros. GLEW defines the same symbols. **They must never appear in the same translation unit.**

The avatar system uses pImpl to enforce this:
- `AvatarRenderer.h` — no GL includes; safe to include from `main.cpp`
- `AvatarRenderer.cpp` — includes `<GL/glew.h>`; never include ImGui backends here

**Rule:** Any new file that uses GLEW must not include `<imgui_impl_opengl3.h>` (directly or transitively).

## STB_IMAGE_IMPLEMENTATION

Defined **only** in `src/avatar/GltfLoader.cpp`. Do not define it anywhere else. If other code needs stb_image (e.g. texture loading for camera feed), include `<stb_image.h>` without the implementation define — the linker will find it from GltfLoader.cpp.

## Avatar model

The avatar renderer is fully implemented but requires a GLTF 2.0 model file:
```
resources/models/avatar/avatar.glb
```
Without it the app runs normally with the teal placeholder. See `docs/00-PROJECT-STATUS.md → Immediate Next Steps` for how to obtain a Mixamo model.

## Git hooks

Local hooks live in `.githooks/` (checked into the repo). Activate them after cloning:

```bash
bash tools/setup-hooks.sh
# or manually: git config core.hooksPath .githooks
```

| Hook | What it does |
|------|-------------|
| `pre-commit` | Auto-formats staged `.cpp`/`.h`/`.hpp` files with `clang-format` and re-stages them |
| `commit-msg` | Rejects messages that don't follow Conventional Commits (`feat(scope): ...`) |
| `pre-push` | Runs `cmake --build` and blocks push if the build fails (skips if build dir missing) |

## ML pipeline (not yet implemented)

Models will live in `resources/models/` as ONNX files. Input shape for the gesture classifier: `[1, 60, 225]` (batch, frames, landmarks).
GPU execution: DirectML on Windows, CoreML on macOS, CUDA on Linux — use `#ifdef VISEAR_PLATFORM_*` guards.
MediaPipe landmarks wire into the avatar via `AvatarRenderer::setPose(span<mat4>)`.
See `docs/06-ML-PIPELINE.md` for the full training → export → inference workflow.
