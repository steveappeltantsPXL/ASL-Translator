# C++ Style Rules

## Standard and compiler
- C++20 (`set(CMAKE_CXX_STANDARD 20)`)
- MSVC (Visual Studio 2022), warnings at `/W4`
- Runtime library: `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL` — `/MDd` for Debug, `/MD` for Release.
  Must match vcpkg triplet `x64-windows`. OpenCV's `debug_build_guard` namespace requires
  `_DEBUG` consistency between consumer code and the DLL.
- `/utf-8` compile flag is mandatory — without it MSVC corrupts non-ASCII string literals (► ● ⚙ render as `?`)

## Memory management — RAII only
- Never use raw `new` / `delete` — use `std::unique_ptr`, `std::vector`, or stack objects
- No manual memory management for SDL surfaces: always call `SDL_DestroySurface` via RAII or immediately after use

## const-correctness
- Prefer `const&` for function parameters that are not modified
- Use `std::move` when transferring ownership
- Mark member functions `const` when they don't modify state

## Windows-specific headers
- `<windows.h>` must come before `<GL/gl.h>` on MSVC — always keep the `#ifdef _WIN32` guard:
  ```cpp
  #ifdef _WIN32
  #include <windows.h>
  #include <dwmapi.h>
  #endif
  #include <GL/gl.h>
  ```
- Use `#ifdef VISEAR_PLATFORM_WINDOWS` or `#ifdef _WIN32` consistently — never mix styles
- Never include Win32 headers outside a platform guard

## Code quality
- Use `constexpr` over `#define` for constants
- Use `[[nodiscard]]` on functions whose return value must not be discarded
- Avoid `using namespace std` in headers
- Prefer `spdlog` for logging over `printf` or `std::cout`
- Use `nlohmann::json` for JSON parsing, `SQLiteCpp` for database access

## Supplementary Unicode
- Supplementary-plane emoji (U+1F000+) cannot render in ImGui — use BMP Unicode symbols (≤ U+FFFF) or `ImDrawList` primitives
