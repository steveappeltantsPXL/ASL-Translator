# Project Structure & CLion Setup

## Directory Layout

```
visear-asl-translator/
│
├── CMakeLists.txt                    # Root CMake configuration
├── vcpkg.json                        # vcpkg dependency manifest
├── .clion/                           # CLion-specific settings
│   └── cmake-profiles.xml
├── .github/
│   └── workflows/
│       ├── build-windows.yml
│       ├── build-macos.yml
│       └── build-linux.yml
│
├── src/                              # Application source code
│   ├── main.cpp                      # Entry point
│   │
│   ├── app/                          # Application core
│   │   ├── Application.h
│   │   ├── Application.cpp           # SDL init, main loop, shutdown
│   │   ├── Config.h
│   │   └── Config.cpp                # Runtime configuration / user settings
│   │
│   ├── ui/                           # Dear ImGui UI layer
│   │   ├── UIManager.h
│   │   ├── UIManager.cpp             # ImGui context, rendering orchestration
│   │   ├── panels/
│   │   │   ├── CameraPanel.h/.cpp    # Live camera feed display
│   │   │   ├── CaptionPanel.h/.cpp   # Text output / caption display
│   │   │   ├── ControlPanel.h/.cpp   # Mode selection, settings
│   │   │   ├── DictionaryPanel.h/.cpp# ASL dictionary lookup
│   │   │   ├── AnalyticsPanel.h/.cpp # Local analytics dashboard
│   │   │   └── DebugPanel.h/.cpp     # Landmark overlay, FPS, latency
│   │   └── widgets/
│   │       ├── VideoWidget.h/.cpp    # OpenGL texture rendering for video
│   │       └── WaveformWidget.h/.cpp # Audio level visualization
│   │
│   ├── capture/                      # Input capture
│   │   ├── CameraCapture.h
│   │   ├── CameraCapture.cpp         # OpenCV camera management
│   │   ├── AudioCapture.h
│   │   ├── AudioCapture.cpp          # SDL_Audio / PortAudio microphone input
│   │   └── ScreenCapture.h/.cpp      # Optional: capture screen region
│   │
│   ├── pipeline/                     # Translation pipelines
│   │   ├── PipelineManager.h
│   │   ├── PipelineManager.cpp       # Orchestrates active pipelines
│   │   ├── FrameData.h               # Shared data structures between stages
│   │   │
│   │   ├── asl/                      # ASL → English pipeline
│   │   │   ├── LandmarkExtractor.h/.cpp    # MediaPipe pose/hand/face
│   │   │   ├── FeatureProcessor.h/.cpp     # Landmark normalization, windowing
│   │   │   ├── GestureClassifier.h/.cpp    # ONNX Runtime inference
│   │   │   └── SentenceAssembler.h/.cpp    # Token sequence → English text
│   │   │
│   │   ├── stt/                      # Speech → Text pipeline
│   │   │   ├── WhisperEngine.h/.cpp        # whisper.cpp integration
│   │   │   └── TranscriptBuffer.h/.cpp     # Manages ongoing transcription
│   │   │
│   │   ├── tts/                      # Text → Speech pipeline
│   │   │   ├── TTSEngine.h/.cpp            # Piper / Sherpa-onnx integration
│   │   │   └── AudioOutputBuffer.h/.cpp    # Audio queue management
│   │   │
│   │   └── sta/                      # Speech/Text → ASL pipeline
│   │       ├── TextToGloss.h/.cpp          # English → ASL gloss mapping
│   │       └── GlossRenderer.h/.cpp        # Display gloss or drive avatar
│   │
│   ├── ml/                           # ML runtime management
│   │   ├── ONNXRuntime.h
│   │   ├── ONNXRuntime.cpp           # ONNX session management, GPU providers
│   │   ├── ModelManager.h
│   │   ├── ModelManager.cpp          # Model loading, versioning, hot-swap
│   │   └── models/                   # Model metadata (actual .onnx files in resources)
│   │       └── model_registry.json
│   │
│   ├── output/                       # Output integrations
│   │   ├── VirtualCamera.h
│   │   ├── VirtualCamera.cpp         # Virtual camera driver abstraction
│   │   ├── VirtualMicrophone.h
│   │   ├── VirtualMicrophone.cpp     # Virtual mic driver abstraction
│   │   ├── CaptionOverlay.h
│   │   ├── CaptionOverlay.cpp        # Render text onto video frames
│   │   └── platform/
│   │       ├── VCamWindows.h/.cpp    # DirectShow / Media Foundation
│   │       ├── VCamMacOS.h/.cpp      # CoreMediaIO plugin
│   │       └── VCamLinux.h/.cpp      # v4l2loopback
│   │
│   ├── network/                      # API client (talks to backend)
│   │   ├── APIClient.h
│   │   ├── APIClient.cpp             # REST client for backend services
│   │   ├── ModelSync.h
│   │   └── ModelSync.cpp             # Model update checks and downloads
│   │
│   └── utils/                        # Shared utilities
│       ├── ThreadPool.h/.cpp         # Task-based parallelism
│       ├── RingBuffer.h              # Lock-free ring buffer for audio/video
│       ├── Timer.h                   # High-resolution timing / profiling
│       ├── Logger.h/.cpp             # Structured logging (spdlog wrapper)
│       └── Platform.h               # OS detection macros
│
├── resources/                        # Runtime resources
│   ├── models/
│   │   ├── gesture_classifier.onnx
│   │   ├── pose_estimation.onnx
│   │   ├── whisper-base.bin
│   │   └── piper-en-us.onnx
│   ├── fonts/
│   │   └── NotoSans-Regular.ttf      # ImGui font
│   ├── icons/
│   │   └── visear-icon.png
│   └── dictionaries/
│       └── asl_lexicon.db            # SQLite ASL dictionary
│
├── training/                         # Python ML training (separate environment)
│   ├── requirements.txt
│   ├── README.md
│   ├── train_gesture_model.py
│   ├── train_pose_model.py
│   ├── export_onnx.py
│   ├── evaluate.py
│   ├── data/
│   │   ├── download_wlasl.py
│   │   ├── preprocess.py
│   │   └── augment.py
│   └── notebooks/
│       ├── exploration.ipynb
│       └── model_comparison.ipynb
│
├── tests/                            # C++ tests
│   ├── CMakeLists.txt
│   ├── test_gesture_classifier.cpp
│   ├── test_landmark_extractor.cpp
│   ├── test_whisper_engine.cpp
│   ├── test_tts_engine.cpp
│   ├── test_pipeline_latency.cpp
│   └── test_data/
│       └── sample_landmarks.json
│
├── docs/                             # Documentation (these files)
│   ├── 01-ARCHITECTURE-OVERVIEW.md
│   ├── 02-PROJECT-STRUCTURE.md
│   └── ...
│
└── scripts/                          # Build and utility scripts
    ├── setup-vcpkg.sh
    ├── download-models.sh
    └── package/
        ├── package-windows.bat
        ├── package-macos.sh
        └── package-linux.sh
```

---

## CMake Configuration

### Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.24)
project(VisearASLTranslator VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # For CLion code analysis

# ── vcpkg integration ──
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

# ── Platform detection ──
if(WIN32)
    add_compile_definitions(VISEAR_PLATFORM_WINDOWS)
elseif(APPLE)
    add_compile_definitions(VISEAR_PLATFORM_MACOS)
else()
    add_compile_definitions(VISEAR_PLATFORM_LINUX)
endif()

# ── Dependencies via vcpkg ──
find_package(SDL3 CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(onnxruntime CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# ── Dear ImGui (vendored, not in vcpkg) ──
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
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# ── MediaPipe (custom find module or prebuilt) ──
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(MediaPipe REQUIRED)

# ── whisper.cpp (add as subdirectory or prebuilt) ──
add_subdirectory(vendor/whisper.cpp)

# ── Piper TTS ──
add_subdirectory(vendor/piper)

# ── Main application ──
file(GLOB_RECURSE APP_SOURCES "src/*.cpp" "src/*.h")
add_executable(${PROJECT_NAME} ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    SDL3::SDL3
    opencv_core opencv_videoio opencv_imgproc opencv_highgui
    onnxruntime::onnxruntime
    MediaPipe::MediaPipe
    whisper
    piper
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    SQLiteCpp
)

# ── Copy resources to build directory ──
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
)

# ── Tests ──
enable_testing()
add_subdirectory(tests)
```

### vcpkg.json

```json
{
  "name": "visear-asl-translator",
  "version": "0.1.0",
  "dependencies": [
    "sdl3",
    "opencv4",
    "onnxruntime-gpu",
    "spdlog",
    "nlohmann-json",
    "sqlitecpp",
    "gtest",
    "cpr",
    "protobuf"
  ]
}
```

---

## CLion Configuration

### CMake Profiles

In CLion, configure via **Settings → Build, Execution, Deployment → CMake**:

| Profile        | Build Type     | CMake Options                                             | Use For                      |
| -------------- | -------------- | --------------------------------------------------------- | ---------------------------- |
| Debug          | Debug          | `-DCMAKE_BUILD_TYPE=Debug`                                | Daily development            |
| Release        | Release        | `-DCMAKE_BUILD_TYPE=Release -DVISEAR_ENABLE_PROFILING=ON` | Performance testing          |
| RelWithDebInfo | RelWithDebInfo | `-DCMAKE_BUILD_TYPE=RelWithDebInfo`                       | Debugging performance issues |

### Recommended CLion Plugins

- **CMake Plus** — enhanced CMake syntax support
- **Protocol Buffers** — if using protobuf for API/gRPC
- **Python** — for working in the training/ directory
- **Markdown** — for documentation editing
- **GitToolBox** — enhanced git integration

### Run Configurations

Create these run/debug configurations in CLion:

**Main Application:**

```
Name: Visear ASL Translator
Target: VisearASLTranslator
Program arguments: --config resources/default_config.json
Working directory: $ProjectFileDir$/build/Debug
Environment: SPDLOG_LEVEL=debug
```

**Unit Tests:**

```
Name: All Tests
Target: VisearTests
Working directory: $ProjectFileDir$/build/Debug
```

**Latency Benchmark:**

```
Name: Pipeline Latency Test
Target: VisearTests
Program arguments: --gtest_filter=*Latency*
```

### Code Style

In **Settings → Editor → Code Style → C/C++**, import a `.clang-format` from the project root:

```yaml
# .clang-format
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Inline
BreakBeforeBraces: Attach
PointerAlignment: Left
```

### CLion Performance Tips

- Enable **Settings → Build → Compilation Database** for faster indexing
- Set **File → Power Save Mode** off during active coding
- Increase heap: **Help → Change Memory Settings** → 4096 MB for large projects
- Use **File → Invalidate Caches** if indexing becomes stale after branch switches

---

## Visual Studio & MSVC Setup (Windows)

### Installation

1.  **Visual Studio 2022**: Install the "Desktop development with C++" workload.
2.  **vcpkg**: Ensure `VCPKG_ROOT` is set in your system environment variables.
3.  **CMake**: Usually bundled with VS, but a standalone installation is recommended for CLI consistency.

### CMake Generation

To generate a Visual Studio Solution (verified working on Windows 11):

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows
```

> Always specify `-A x64` and `-DVCPKG_TARGET_TRIPLET=x64-windows`.
> Omitting these causes CMake to default to x86, which breaks 64-bit packages.

Open `build/VisearASLTranslator.sln` to start developing.

Build from command line:

```powershell
cmake --build build --config Debug
.\build\Debug\VisearASLTranslator.exe
```

### Recommended Extensions

- **C++ Build Insights** — for analyzing build times.
- **Markdown Editor** — for editing project documentation.
- **GitHub Copilot** — for AI-assisted coding.

### Debugging & Profiling

- Use **Local Windows Debugger** to run the app.
- Use **Performance Profiler (Alt+F2)** for bottleneck analysis, specifically the "CPU Usage" and "C++ Memory Usage" tools.

---

---

## Build Instructions

### Prerequisites

```bash
# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && ./bootstrap-vcpkg.sh
export VCPKG_ROOT=$(pwd)

# Install system dependencies (Linux)
sudo apt install build-essential cmake ninja-build \
    libgl1-mesa-dev libglu1-mesa-dev \
    libx11-dev libxrandr-dev libxi-dev

# Install system dependencies (macOS)
brew install cmake ninja

# Install system dependencies (Windows)
# Visual Studio 2022 with C++ Desktop workload
```

### First Build

```bash
# Clone and setup
git clone https://github.com/visear/asl-translator.git
cd asl-translator

# Vendor Dear ImGui
git submodule add https://github.com/ocornut/imgui.git vendor/imgui
git submodule add https://github.com/ggerganov/whisper.cpp.git vendor/whisper.cpp
git submodule add https://github.com/rhasspy/piper.git vendor/piper

# Install vcpkg dependencies
$VCPKG_ROOT/vcpkg install

# Download ML models
./scripts/download-models.sh

# Configure and build
cmake -B build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run
./build/VisearASLTranslator
```

### In CLion

1. Open the project root (CLion detects CMakeLists.txt automatically)
2. Set **Settings → Build → Toolchains** → your compiler (GCC/Clang/MSVC)
3. Set **Settings → Build → CMake** → add `-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`
4. CLion will index and configure automatically
5. Select the `VisearASLTranslator` target and press Run

---

## Git Workflow

### Branch Strategy

```
main                    ← Stable releases only
├── develop             ← Integration branch
│   ├── feature/asl-pipeline
│   ├── feature/stt-integration
│   ├── feature/virtual-camera
│   └── feature/imgui-panels
└── release/v0.1.0      ← Release candidates
```

### .gitignore

```gitignore
build/
.cache/
.idea/
cmake-build-*/
vendor/
resources/models/*.onnx
resources/models/*.bin
*.pyc
__pycache__/
training/data/raw/
training/data/processed/
.env
```

Large model files should be tracked with **Git LFS** or downloaded via the setup script rather than committed to the repository.
