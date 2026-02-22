# Build Commands — Visear ASL Translator

All commands are run from the **project root**:
`C:\Users\steve\AppDev\Projects\Visear-ASL-Translator\`

> **Note:** This document reflects the actual verified setup on Windows 11 with Visual Studio 2022.
> Use the Visual Studio generator — **not Ninja** — on Windows. See gotchas below.

---

## 1. One-Time Setup

### Verified installed versions (Windows 11)

| Tool              | Version   | Status     |
| ----------------- | --------- | ---------- |
| Visual Studio     | 2022 v17  | Required   |
| CMake             | 3.31.6    | Installed  |
| Ninja             | 1.12.1    | Installed* |
| Python            | 3.14.2    | Installed  |
| vcpkg             | Latest    | Installed  |
| Git               | Latest    | Installed  |

*Ninja is installed but **not used on Windows** — the VS 2022 generator is used instead. See section 4.

### Install prerequisites (PowerShell — run once)

```powershell
winget install Kitware.CMake
winget install Ninja-build.Ninja
winget install Python.Python.3.14.2
```

### Install vcpkg (run once)

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
[System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
```

> Restart your terminal after setting `VCPKG_ROOT`.

### Verify vcpkg

```powershell
echo $env:VCPKG_ROOT
# Expected: C:\vcpkg

C:\vcpkg\vcpkg --version
```

---

## 2. Initialize Git & Add Vendor Submodules

The project root must be a git repository before submodules can be added.

```powershell
# Initialize git (if not already done)
git init

# Add Dear ImGui (docking branch — required for dockable panels)
git submodule add -b docking https://github.com/ocornut/imgui.git vendor/imgui

# These are commented out in CMakeLists.txt until ready to integrate:
# git submodule add https://github.com/ggerganov/whisper.cpp.git vendor/whisper.cpp
# git submodule add https://github.com/rhasspy/piper.git vendor/piper
```

> **Gotcha:** `vendor/` must NOT be listed in `.gitignore`. Git submodules are
> registered via `.gitmodules` and cannot be added to ignored paths.

The `vendor/tinygltf/` directory is **not** a submodule — it contains vendored
header files (`tiny_gltf.h`, `stb_image.h`) committed directly to the repository.

If cloning an existing repo that already has submodules:

```powershell
git submodule update --init --recursive
```

---

## 3. vcpkg.json — Required Fields

The `vcpkg.json` manifest **must** include a `builtin-baseline` as a 40-character
commit SHA. A date string will be rejected.

```json
{
  "name": "visear-asl-translator",
  "version": "0.1.0",
  "builtin-baseline": "3af1d1e60af2b2abf55760538cd607829029b07a",
  "dependencies": [
    "sdl3", "sdl3-image", "opencv4", "onnxruntime-gpu", "spdlog",
    "nlohmann-json", "sqlitecpp", "gtest", "cpr", "protobuf",
    "glm", "glew"
  ]
}
```

If you need the current baseline SHA for your vcpkg clone:

```powershell
git -C C:\vcpkg rev-parse HEAD
```

---

## 4. Configure & Build (Windows — Visual Studio Generator)

> **Important:** Use the `"Visual Studio 17 2022"` generator on Windows.
> The Ninja generator defaults to x86 with MSVC, causing package mismatches
> (SDL3, ONNX Runtime GPU are 64-bit only). The `-A x64` flag is not required
> when VCPKG_TARGET_TRIPLET is set to `x64-windows` — CMake infers the arch.

```powershell
# Configure (creates cmake-build-debug-msvc/ with .sln and .vcxproj files)
cmake -B cmake-build-debug-msvc -G "Visual Studio 17 2022" `
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows

# Build Debug
cmake --build cmake-build-debug-msvc --config Debug

# Run
.\cmake-build-debug-msvc\Debug\VisearASLTranslator.exe
```

```powershell
# Build Release
cmake --build cmake-build-debug-msvc --config Release

.\cmake-build-debug-msvc\Release\VisearASLTranslator.exe
```

Opening the solution in Visual Studio:

```powershell
start cmake-build-debug-msvc\VisearASLTranslator.sln
```

---

## 5. Avatar Model (one-time setup)

The avatar renderer is implemented but requires a GLTF 2.0 model file:

```
resources/models/avatar/avatar.glb
```

To obtain a free Mixamo character:

1. Visit [mixamo.com](https://www.mixamo.com) → choose a character → **T-Pose** → Download as **FBX for Unity**
2. Open the FBX in **Blender** → **File → Export → glTF 2.0** (`.glb`, embed textures, include armature)
3. Save as `resources/models/avatar/avatar.glb` in the project root

Alternatively, download a pre-rigged `.glb` from the
[Khronos glTF Sample Assets](https://github.com/KhronosGroup/glTF-Sample-Assets).

Without the model file the app runs normally — the avatar panel shows the teal
placeholder and logs: `Avatar init failed — placeholder will be shown`.

---

## 6. onnxruntime-gpu — CMake Quirk

The `onnxruntime-gpu` vcpkg package does **not** ship a CMake config file.
`find_package(onnxruntime CONFIG REQUIRED)` will fail.

The `CMakeLists.txt` handles this with manual `find_path` / `find_library` calls:

```cmake
find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    PATHS "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/include"
    REQUIRED)
find_library(ONNXRUNTIME_LIB onnxruntime
    PATHS "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/lib"
    REQUIRED)
```

No action needed — this is already in the project's `CMakeLists.txt`.

---

## 7. Run Tests

> Tests are not yet implemented. Uncomment the GTest block in `CMakeLists.txt`
> and add `tests/CMakeLists.txt` before using these commands.

```powershell
cmake --build cmake-build-debug-msvc --config Debug --target VisearTests
ctest --test-dir cmake-build-debug-msvc -C Debug --output-on-failure
```

---

## 8. Clean Build

Required after changing `find_package` calls or linking structure in `CMakeLists.txt`:

```powershell
Remove-Item -Recurse -Force cmake-build-debug-msvc

cmake -B cmake-build-debug-msvc -G "Visual Studio 17 2022" `
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build cmake-build-debug-msvc --config Debug
```

---

## 9. Replacing the Application Icon

```powershell
# 1. Drop your new image (any size PNG) as src/assets/app_icon.ico
# 2. Convert it to a proper multi-resolution ICO:
powershell -ExecutionPolicy Bypass -File tools\make_ico.ps1
# 3. Rebuild — the RC compiler picks it up automatically:
cmake --build cmake-build-debug-msvc --config Debug
```

---

## 10. Common Errors & Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| `builtin-baseline` is not a valid commit sha | Date string used instead of SHA | Use 40-char hex SHA from `git -C C:\vcpkg rev-parse HEAD` |
| `onnxruntime-gpu is only supported on ... x86-windows` | Ninja defaults to x86 | Use `"Visual Studio 17 2022"` generator |
| `Could not find onnxruntimeConfig.cmake` | onnxruntime-gpu has no CMake config | Already handled in CMakeLists.txt via find_library |
| `Cannot find source file: vendor/imgui/imgui.cpp` | ImGui submodule not cloned | Run `git submodule update --init --recursive` |
| `The following paths are ignored by .gitignore: vendor` | vendor/ was in .gitignore | Remove `vendor/` line from .gitignore |
| `CMAKE_MAKE_PROGRAM is not set` | Ninja not found or wrong generator | Use VS generator, not Ninja, on Windows |
| `Cannot open include file: 'stb_image.h'` | stb_image.h missing from vendor/tinygltf/ | Copy `stb_image.h` from stb repo into `vendor/tinygltf/` |
| Avatar FBO incomplete `0x8CD6` | GL context too old (< 3.3) | Verify `SDL_GL_CONTEXT_MINOR_VERSION` is set to 3 |
| `reinterpret_cast: cannot convert from 'uintptr_t' to 'ImTextureID'` | ImTextureID is ImU64 in this ImGui build | Use `static_cast<ImTextureID>(texId)` |
| `RC2175: resource file assets\app_icon.ico is not in 3.00 format` | ICO file is actually a PNG | Run `tools\make_ico.ps1` to convert |

---

## Notes

- First-time vcpkg install (building from source) takes **15–45 minutes**. Subsequent runs use the binary cache and take seconds.
- vcpkg packages are cached at `C:\Users\<user>\AppData\Local\vcpkg\archives`.
- `whisper.cpp`, `piper`, and `MediaPipe` are commented out in `CMakeLists.txt` until their submodules are added and integrated.
- `VCPKG_ROOT` must be set as a persistent user/system environment variable, not just for the current session.
- `vendor/tinygltf/` contains `tiny_gltf.h` and `stb_image.h` committed directly — not a git submodule.
