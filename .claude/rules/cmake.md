# CMake & vcpkg Rules

## Generator
- Always use **Visual Studio 17 2022** on Windows — never Ninja
- Ninja + MSVC defaults to x86; SDL3 and ONNX Runtime are 64-bit only
- CLion build directory: `cmake-build-debug-msvc/`

## vcpkg.json
- `builtin-baseline` must be a 40-character commit SHA — never a date string
  - Get the current SHA: `git -C C:\vcpkg rev-parse HEAD`
- When adding a dependency: update **both** `vcpkg.json` **and** `CMakeLists.txt`
  (`find_package` + `target_link_libraries`)
- Current dependencies: `sdl3`, `sdl3-image`, `opencv4`, `onnxruntime-gpu`, `spdlog`,
  `nlohmann-json`, `sqlitecpp`, `protobuf`, `cpr`, `gtest`

## find_package patterns
- Standard vcpkg packages: `find_package(SDL3 CONFIG REQUIRED)`
- **ONNX Runtime exception:** ships no CMake config — use `find_path` / `find_library`
  as already set up in `CMakeLists.txt`. Never use `find_package(onnxruntime CONFIG REQUIRED)`.
- ImGui is a vendored static lib in `vendor/imgui/` (docking branch) — never add it to vcpkg

## target_link_libraries
- Link order matters on MSVC: list dependencies before their dependents
- Windows-only libs go inside `if(WIN32)`:
  ```cmake
  if(WIN32)
      target_link_libraries(${PROJECT_NAME} PRIVATE opengl32 dwmapi)
  endif()
  ```

## Windows resource file
- `src/app.rc` is included in `APP_SOURCES` on WIN32 — embeds `app_icon.ico` into the .exe
- To replace the icon: update `src/assets/app_icon.ico` and rebuild — RC compiler handles the rest

## Planned but disabled
- `whisper.cpp` and `piper` blocks are commented out in `CMakeLists.txt`
- Uncomment only after adding their git submodules (`git submodule add ...`)

## After CMakeLists changes
- Always do a clean reconfigure if changing `find_package` or linking structure:
  delete `cmake-build-debug-msvc/` and re-run configure
