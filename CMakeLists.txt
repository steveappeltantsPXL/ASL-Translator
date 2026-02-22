cmake_minimum_required(VERSION 3.24)

# ── vcpkg integration ──────────────────────────────────────────────────────────
# Must be set BEFORE project() so CMake loads the toolchain during compiler init.
# FORCE overrides any stale value in CMakeCache.txt (e.g. VS bundled vcpkg path).
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain" FORCE)
    # The vcpkg toolchain defaults to ${CMAKE_SOURCE_DIR}/vcpkg_installed, but CLion
    # installs packages into the build directory. Point it at the right location.
    set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed"
        CACHE PATH "vcpkg installed directory" FORCE)
endif()

project(VisearASLTranslator VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Platform detection ─────────────────────────────────────────────────────────
if(WIN32)
    add_compile_definitions(VISEAR_PLATFORM_WINDOWS)
elseif(APPLE)
    add_compile_definitions(VISEAR_PLATFORM_MACOS)
else()
    add_compile_definitions(VISEAR_PLATFORM_LINUX)
endif()

# ── MSVC Configuration ────────────────────────────────────────────────────────
if(MSVC)
    # Debug: Full debug info, no optimization, runtime checks
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od /RTC1")

    # RelWithDebInfo: Debug symbols + optimization (for profiling)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Zi /O2")

    # Runtime library: Use dynamic multi-threaded (matches vcpkg default)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

    # Additional optimizations for profiling accuracy
    add_compile_options(/fp:precise)  # Precise floating-point for consistent results

    # Treat source files and emitted string literals as UTF-8.
    # Without this MSVC uses the system code page (CP1252), which silently
    # corrupts any non-ASCII character (▶ ⏹ ⚙ ● …) in string literals.
    add_compile_options(/utf-8)

    # Suppress some MSVC warnings (optional)
    add_compile_options(/W4)  # Warning level 4
endif()

# ── vcpkg package search path ─────────────────────────────────────────────────
# Explicitly prepend the vcpkg installed directory to CMAKE_PREFIX_PATH.
# This is required when CLion's cmake process doesn't inherit VCPKG_ROOT from
# the system environment (so the vcpkg toolchain above may not have run),
# and also covers the case where CLion installs packages into the build dir.
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows")  # default for this Windows x64 project
endif()
foreach(_vcpkg_prefix
        "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}"
        "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
    if(EXISTS "${_vcpkg_prefix}")
        list(PREPEND CMAKE_PREFIX_PATH "${_vcpkg_prefix}")
    endif()
endforeach()

# ── vcpkg-managed dependencies ─────────────────────────────────────────────────
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)

# ── ONNX Runtime (vcpkg onnxruntime-gpu ships no CMake config — link manually) ─
find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    PATHS "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/include"
    REQUIRED)
find_library(ONNXRUNTIME_LIB onnxruntime
    PATHS "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/lib"
    REQUIRED)
message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIB}")

# ── Dear ImGui (vendored submodule — docking branch) ──────────────────────────
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# ── whisper.cpp (vendored submodule) ──────────────────────────────────────────
# Uncomment once: git submodule add https://github.com/ggerganov/whisper.cpp.git vendor/whisper.cpp
# add_subdirectory(vendor/whisper.cpp)

# ── Piper TTS (vendored submodule) ────────────────────────────────────────────
# Uncomment once: git submodule add https://github.com/rhasspy/piper.git vendor/piper
# add_subdirectory(vendor/piper)

# ── MediaPipe (custom find module or prebuilt) ────────────────────────────────
# Uncomment once cmake/FindMediaPipe.cmake is in place and MediaPipe is built:
# list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
# find_package(MediaPipe REQUIRED)

# ── Main application ──────────────────────────────────────────────────────────
file(GLOB_RECURSE APP_SOURCES "src/*.cpp" "src/*.h")
# Embed the application icon on Windows via a resource file
if(WIN32)
    list(APPEND APP_SOURCES src/app.rc)
endif()
add_executable(${PROJECT_NAME} ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor/tinygltf
    ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    SDL3::SDL3
    SDL3_image::SDL3_image
    opencv_core
    opencv_videoio
    opencv_imgproc
    opencv_highgui
    ${ONNXRUNTIME_LIB}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    SQLiteCpp
    glm::glm
    GLEW::GLEW
    # whisper          # uncomment when submodule is added
    # piper            # uncomment when submodule is added
    # MediaPipe::MediaPipe  # uncomment when MediaPipe is configured
)

# Windows: link OpenGL + DWM (accent color query)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32 dwmapi)
endif()

# ── Copy resources to build output directory ──────────────────────────────────
if(EXISTS ${CMAKE_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
    )
endif()

# ── Copy assets (icons, images) to build output directory ─────────────────────
if(EXISTS ${CMAKE_SOURCE_DIR}/src/assets)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/src/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
# Uncomment once tests/CMakeLists.txt exists:
# find_package(GTest CONFIG REQUIRED)
# enable_testing()
# add_subdirectory(tests)
