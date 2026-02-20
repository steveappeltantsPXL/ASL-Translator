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
    "sdl3", "opencv4", "onnxruntime-gpu", "spdlog",
    "nlohmann-json", "sqlitecpp", "gtest", "cpr", "protobuf"
  ]
}
```

If you need the current baseline SHA for your vcpkg clone:

```powershell
git -C C:\vcpkg rev-parse HEAD
```

---

## 4. Configure & Build (Windows — Visual Studio Generator)

> **Important:** Use the `"Visual Studio 17 2022" -A x64` generator on Windows.
> The Ninja generator defaults to x86 with MSVC, causing package mismatches
> (SDL3, ONNX Runtime GPU are 64-bit only).

```powershell
# Configure (creates build/ with .sln and .vcxproj files)
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows

# Build Debug
cmake --build build --config Debug

# Run
.\build\Debug\VisearASLTranslator.exe
```

```powershell
# Build Release
cmake --build build --config Release

.\build\Release\VisearASLTranslator.exe
```

Opening the solution in Visual Studio:

```powershell
# Open the generated solution directly
start build\VisearASLTranslator.sln
```

---

## 5. onnxruntime-gpu — CMake Quirk

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

## 6. Run Tests

> Tests are not yet implemented. Uncomment the GTest block in `CMakeLists.txt`
> and add `tests/CMakeLists.txt` before using these commands.

```powershell
cmake --build build --config Debug --target VisearTests
ctest --test-dir build -C Debug --output-on-failure
```

---

## 7. Clean Build

```powershell
Remove-Item -Recurse -Force build

cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build --config Debug
```

---

## 8. Common Errors & Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| `builtin-baseline` is not a valid commit sha | Date string used instead of SHA | Use 40-char hex SHA from `git -C C:\vcpkg rev-parse HEAD` |
| `onnxruntime-gpu is only supported on ... x86-windows` | Ninja defaults to x86 | Use `"Visual Studio 17 2022" -A x64` generator |
| `Could not find onnxruntimeConfig.cmake` | onnxruntime-gpu has no CMake config | Already handled in CMakeLists.txt via find_library |
| `Cannot find source file: vendor/imgui/imgui.cpp` | ImGui submodule not added | Run `git submodule add -b docking ... vendor/imgui` |
| `The following paths are ignored by .gitignore: vendor` | vendor/ was in .gitignore | Remove `vendor/` line from .gitignore |
| `CMAKE_MAKE_PROGRAM is not set` | Ninja not found or wrong generator | Use VS generator, not Ninja, on Windows |

---

## Notes

- First-time vcpkg install (building from source) takes **15–45 minutes**. Subsequent runs use the binary cache and take seconds.
- vcpkg packages are cached at `C:\Users\<user>\AppData\Local\vcpkg\archives`.
- `whisper.cpp`, `piper`, and `MediaPipe` are commented out in `CMakeLists.txt` until their submodules are added and integrated.
- `VCPKG_ROOT` must be set as a persistent user/system environment variable, not just for the current session.
