cmake_minimum_required(VERSION 3.24)
project(VisearASLTranslator VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── vcpkg integration ──────────────────────────────────────────────────────────
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

# ── Platform detection ─────────────────────────────────────────────────────────
if(WIN32)
    add_compile_definitions(VISEAR_PLATFORM_WINDOWS)
elseif(APPLE)
    add_compile_definitions(VISEAR_PLATFORM_MACOS)
else()
    add_compile_definitions(VISEAR_PLATFORM_LINUX)
endif()

# ── vcpkg-managed dependencies ─────────────────────────────────────────────────
find_package(SDL3 CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)

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
add_executable(${PROJECT_NAME} ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    SDL3::SDL3
    opencv_core
    opencv_videoio
    opencv_imgproc
    opencv_highgui
    ${ONNXRUNTIME_LIB}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    SQLiteCpp
    # whisper          # uncomment when submodule is added
    # piper            # uncomment when submodule is added
    # MediaPipe::MediaPipe  # uncomment when MediaPipe is configured
)

# Windows: link OpenGL
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
endif()

# ── Copy resources to build output directory ──────────────────────────────────
if(EXISTS ${CMAKE_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
    )
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
# Uncomment once tests/CMakeLists.txt exists:
# find_package(GTest CONFIG REQUIRED)
# enable_testing()
# add_subdirectory(tests)
