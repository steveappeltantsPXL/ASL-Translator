# Session Summary — 2026-02-22

This file captures what was built, decided, and left pending in this working session.
Start here when resuming development.

---

## What Was Done This Session

### 1. Tier 2 Rigged 3D Avatar — Complete OpenGL Rendering Pipeline

**New files created (`src/avatar/`):**

| File | Purpose |
|------|---------|
| `AvatarRenderer.h/.cpp` | pImpl façade — GLEW init, FBO, per-frame render, `setPose()` for MediaPipe |
| `AvatarShaders.h` | GLSL 3.30 skinned-mesh vertex shader + Phong fragment shader (inline strings) |
| `GltfLoader.h/.cpp` | tinygltf GLB loader — mesh primitives, skeleton, inverse bind matrices, animations |
| `SkinnedMesh.h/.cpp` | VAO/VBO/IBO with `glVertexAttribIPointer` for integer bone IDs (requires GL 3.3) |
| `Skeleton.h/.cpp` | Joint hierarchy — `computeSkinMatrices()` via parent-before-child traversal |
| `AnimationPlayer.h/.cpp` | Keyframe sampler (linear T/S, slerp R), idle animation loop |

**New vendored headers:**
- `vendor/tinygltf/tiny_gltf.h` — v2.9.3, downloaded from GitHub
- `vendor/tinygltf/stb_image.h` — required by tinygltf for embedded texture decoding

**New directory created:**
- `resources/models/avatar/` — place `avatar.glb` here to see the avatar

### 2. Build System Updates

- `vcpkg.json` — added `glm` (math) and `glew` (GL extension loader)
- `CMakeLists.txt` — `find_package` for glm/GLEW, `vendor/tinygltf` include path, linked `glm::glm` and `GLEW::GLEW`
- GL context bumped: **3.0 → 3.3 core** (`SDL_GL_CONTEXT_PROFILE_CORE`)
- GLSL version string: `#version 130` → `#version 330 core`

### 3. main.cpp Changes

- Avatar `#include "avatar/AvatarRenderer.h"`
- `avatarRenderer.init("resources/models/avatar/avatar.glb")` after ImGui init
- `avatarRenderer.render(panelW, panelH, io.DeltaTime)` before `ImGui::NewFrame()` each frame
- Avatar panel: `ImGui::Image(getTexture(), avail, {0,1}, {1,0})` with UV flip (GL y-origin)
- `avatarRenderer.shutdown()` before `ImGui_ImplOpenGL3_Shutdown()` (GL context must be current)
- Falls back to teal placeholder if `avatar.glb` is not found

### 4. .gitignore Fix

- Added patterns: `**/x64/`, `**/x86/`, `*.recipe`, `*.lastbuildstate`, `*.tlog`, `*.idb`
- Untracked two committed MSBuild artifacts:
  - `Visear-ASL-Translator/x64/Debug/Visear-ASL-Translator.exe.recipe`
  - `Visear-ASL-Translator/x64/Debug/Visear-A.65ae1461.tlog/Visear-ASL-Translator.lastbuildstate`

### 5. Documentation Updated

- `docs/00-PROJECT-STATUS.md` — full rewrite reflecting current state
- `docs/03-BUILD-COMMANDS.md` — corrected build dir, added avatar model setup, new error table entries
- `docs/04-ARCHITECTURE-OVERVIEW.md` — avatar renderer section, technology stack status column
- `docs/07-APPLICATION-GUIDE.md` — updated lifecycle, avatar API docs, GL isolation rule, UV flip, ImTextureID cast fix
- `CLAUDE.md` — updated source layout, GL context note, GL loader isolation rule, STB_IMAGE rule, avatar model note

### 6. Also Created

- `.claude/commands/run.md` — `/run` slash command to launch the app (check PID, optional restart)
- `.claude/commands/rebuild.md` was already present — `/rebuild` kills, builds, offers to launch

---

## Build State at End of Session

```
cmake-build-debug-msvc/  ← clean reconfigure completed (vcpkg installed glm + glew)
Debug/VisearASLTranslator.exe  ← builds cleanly, no errors or warnings
```

The app launches and runs. The avatar panel shows the **teal placeholder** because
`resources/models/avatar/avatar.glb` does not yet exist.

---

## Immediate Next Step (before anything else)

**Get the Mixamo avatar model:**

1. Go to [mixamo.com](https://www.mixamo.com) → choose a character → **T-Pose** → Download **FBX for Unity**
2. Open in **Blender** → **File → Export → glTF 2.0** (`.glb`, embed textures, include armature + animations)
3. Save as `resources/models/avatar/avatar.glb`
4. Run `/rebuild` → the avatar panel will show a live animated 3D humanoid

Alternative: grab a pre-converted `.glb` from:
https://github.com/KhronosGroup/glTF-Sample-Assets

---

## Next Development Phase (Phase 2 — Foundation API Layer)

After the avatar model is in place, the next logical tasks are:

1. **`src/capture/CameraCapture.h/.cpp`** — OpenCV `cv::VideoCapture` RAII wrapper
2. **`src/ui/panels/CameraPanel.h/.cpp`** — render live camera frame as GL texture → `ImGui::Image()`
   - Note: `ImTextureID` is `ImU64` in this build — use `static_cast<ImTextureID>(texId)`
   - Camera texture upload: `glTexImage2D` with `CV_COLOR_BGR2RGB` converted frame
3. **`src/app/Application.h/.cpp`** — encapsulate SDL + ImGui + AvatarRenderer lifecycle
4. **`src/ui/UIManager.h/.cpp`** — extract panel layout from `main.cpp`

---

## Known Issues / Pending Commits

The following changes are **staged but not yet committed**:

- All avatar source files (`src/avatar/`)
- `vendor/tinygltf/` headers
- `resources/models/avatar/` directory (empty)
- `vcpkg.json` (glm + glew added)
- `CMakeLists.txt` (glm/GLEW find_package + linker)
- `src/main.cpp` (GL 3.3, AvatarRenderer integration)
- `src/assets/app_icon.ico` (proper ICO format after make_ico.ps1 run)
- `.gitignore` (MSBuild artifact patterns)
- `.claude/commands/run.md`
- All docs updates

**Suggested commit message:**
```
feat(avatar): implement Tier 2 rigged 3D avatar with OpenGL FBO rendering

- Add src/avatar/ — AvatarRenderer, GltfLoader, SkinnedMesh, Skeleton,
  AnimationPlayer, GLSL shaders (skinned mesh + Phong)
- Vendor tinygltf v2.9.3 + stb_image.h in vendor/tinygltf/
- Add glm + glew to vcpkg.json; wire into CMakeLists.txt
- Bump GL context to 3.3 core; update GLSL to #version 330 core
- Integrate AvatarRenderer into main.cpp with FBO UV flip
- Fix .gitignore: exclude MSBuild x64/ artifacts
- Update all docs to reflect current build state
```

---

## Key Technical Decisions Made This Session

| Decision | Rationale |
|----------|-----------|
| pImpl in `AvatarRenderer.h` | Prevents GLEW / ImGui GL loader conflict (both define GL symbols) |
| tinygltf over assimp | Header-only, no vcpkg needed, handles GLTF 2.0 natively |
| glm via vcpkg | Standard for OpenGL math; already used in ImGui internally |
| GLEW for avatar GL calls | FBO + `glVertexAttribIPointer` not in GL 3.0 without extension loader |
| GL 3.3 core context | Required for GLSL 3.30 (integer vertex attributes for bone IDs) |
| `setPose(span<mat4>)` | Ready for MediaPipe without coupling avatar to ML pipeline |
| `static_cast<ImTextureID>` | `ImTextureID` is `ImU64` in this build, not `void*` |
| UV flip `{0,1},{1,0}` | OpenGL FBO y-origin is bottom; ImGui expects top |
| `STB_IMAGE_IMPLEMENTATION` in GltfLoader.cpp | Exactly one TU — avoids multiple definition linker errors |
