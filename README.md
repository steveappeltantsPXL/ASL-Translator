# Visear ASL Translator

A real-time, bidirectional American Sign Language (ASL) translation system. Translates between ASL and spoken/written English in real time, enabling seamless communication between deaf/hard-of-hearing and hearing individuals — including integration with video conferencing platforms.

**Status:** Foundation complete — build system working, app running. Feature development in progress.

---

## Quick Links

- **Status & Architecture:** [docs/00-PROJECT-STATUS.md](docs/00-PROJECT-STATUS.md)
- **Getting Started:** [docs/02-GETTING-STARTED.md](docs/02-GETTING-STARTED.md)
- **Build Commands:** [docs/03-BUILD-COMMANDS.md](docs/03-BUILD-COMMANDS.md)
- **GitHub Workflow:** [docs/01-GITHUB-WORKFLOW.md](docs/01-GITHUB-WORKFLOW.md)
- **Full Documentation:** [docs/](docs/)

---

## Technology Stack

| Component          | Technology                           | Status        |
| ------------------ | ------------------------------------ | ------------- |
| Language           | C++20                                | ✅ Working    |
| Build System       | CMake 3.24+ with Ninja              | ✅ Working    |
| Package Manager    | vcpkg (x64-windows)                 | ✅ Working    |
| UI Framework       | Dear ImGui (docking branch)         | ✅ Running    |
| Windowing/Input    | SDL3                                | ✅ Running    |
| Rendering          | OpenGL 3.0+                         | ✅ Running    |
| ML Inference       | ONNX Runtime (GPU support)          | ✅ Installed  |
| Computer Vision    | OpenCV 4.x                          | ✅ Installed  |
| Hand/Pose Detection| MediaPipe *(not yet integrated)*    | 🔄 Planned    |
| Speech-to-Text     | whisper.cpp *(submodule ready)*     | 🔄 Planned    |
| Text-to-Speech     | Piper *(submodule ready)*           | 🔄 Planned    |
| IDE                | Visual Studio 2022 (MSVC v143)      | ✅ Supported  |
| Testing            | Google Test (GTest)                 | ✅ Installed  |

---

## Project Vision

Six translation pipelines (ASL ↔ Text/Speech, Speech ↔ Text/ASL) enable:

- **Voice Mode:** Deaf user in video call → signs → spoken English output to hearing participants
- **Caption Mode:** Live ASL/speech → text captions for accessibility
- **Full Duplex:** Bidirectional ASL/speech translation in real time
- **Virtual Devices:** Output to Zoom/Teams as virtual camera/microphone/captions

---

## Getting Started (Windows 11)

### One-Time Setup

```powershell
# Install prerequisites
winget install Kitware.CMake
winget install Ninja-build.Ninja

# Install and configure vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
[System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")

# Restart terminal to load VCPKG_ROOT
```

### Clone & Build

```powershell
git clone https://github.com/steveappeltantsPXL/ASL-Translator.git
cd ASL-Translator

# Pull git submodules (Dear ImGui)
git submodule update --init --recursive

# Configure and build
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build --config Debug

# Run
.\build\Debug\VisearASLTranslator.exe
```

**Troubleshooting?** See [docs/03-BUILD-COMMANDS.md](docs/03-BUILD-COMMANDS.md) for detailed build steps and error fixes.

---

## Project Structure

```
Visear-ASL-Translator/
├── src/
│   ├── main.cpp                          # Entry point — SDL3 + ImGui window
│   ├── app/                              # (pending) Application core
│   ├── ui/                               # (pending) ImGui panels
│   ├── capture/                          # (pending) Camera/audio input
│   ├── pipeline/                         # (pending) Translation pipelines
│   ├── ml/                               # (pending) ONNX Runtime wrapper
│   ├── output/                           # (pending) Virtual devices
│   ├── network/                          # (pending) API client
│   └── utils/                            # Logging, threading, profiling
├── vendor/
│   └── imgui/                            # ✅ Git submodule (docking branch)
├── resources/
│   ├── models/                           # ML models (downloaded, not committed)
│   ├── fonts/                            # ImGui fonts
│   ├── icons/                            # UI icons
│   └── dictionaries/                     # ASL lexicon (SQLite)
├── tests/                                # (pending) Google Test suite
├── docs/
│   ├── 00-PROJECT-STATUS.md              # Current state & roadmap
│   ├── 01-GITHUB-WORKFLOW.md             # How to use git/GitHub
│   ├── 02-GETTING-STARTED.md             # Setup guide
│   ├── 03-BUILD-COMMANDS.md              # Build reference
│   ├── 04-ARCHITECTURE-OVERVIEW.md       # System design
│   ├── 05-PROJECT-STRUCTURE.md           # Codebase layout
│   ├── 06-ML-PIPELINE.md                 # ML architecture
│   ├── 07-APPLICATION-GUIDE.md           # App architecture
│   ├── 08-API-SERVER.md                  # Backend design
│   ├── 09-INTEGRATION-GUIDE.md           # Platform integration
│   ├── 10-MOBILE-ROADMAP.md              # Mobile phases
│   └── 11-GITHUB-ADMIN.md                # Repo admin reference
├── CMakeLists.txt                        # Build configuration
├── vcpkg.json                            # Dependency manifest
├── .clang-format                         # Code style (Google, C++20)
├── CONTRIBUTING.md                       # Contribution guidelines
├── SECURITY.md                           # Vulnerability reporting
└── CLA.md                                # Contributor License Agreement
```

---

## What Works Now ✅

- SDL3 window with OpenGL 3.0 rendering
- Dear ImGui docking interface with dark theme
- FPS counter and runtime status display
- CMake build system (Windows x64 verified)
- All vcpkg dependencies installed and CMake-resolvable
- Git submodules (ImGui docking branch)

---

## What's Pending 🔄

| Feature              | Est. Effort | Priority |
| -------------------- | ----------- | -------- |
| OpenCV camera capture| 2-3 days    | High     |
| MediaPipe landmarks  | 1-2 weeks   | High     |
| ONNX model inference | 3-5 days    | High     |
| ASL → Text pipeline  | 1-2 weeks   | High     |
| ImGui panels (6x)    | 2-3 weeks   | High     |
| Virtual camera/mic   | 2-3 weeks   | Medium   |
| whisper.cpp (STT)    | 1 week      | Medium   |
| Piper (TTS)          | 1 week      | Medium   |
| Backend API server   | 3-4 weeks   | Medium   |
| Mobile (Android/iOS) | 12+ weeks   | Low      |

---

## Development Workflow

1. **Read the docs** — start with [docs/00-PROJECT-STATUS.md](docs/00-PROJECT-STATUS.md)
2. **Set up** — follow [docs/02-GETTING-STARTED.md](docs/02-GETTING-STARTED.md)
3. **Contribute** — see [docs/01-GITHUB-WORKFLOW.md](docs/01-GITHUB-WORKFLOW.md) and [CONTRIBUTING.md](CONTRIBUTING.md)
4. **Build** — use [docs/03-BUILD-COMMANDS.md](docs/03-BUILD-COMMANDS.md)

---

## Repository

- **GitHub:** https://github.com/steveappeltantsPXL/ASL-Translator
- **Branch Strategy:** `main` (stable) ← `develop` (integration) ← `feature/*` (your work)
- **License:** Contributor License Agreement required (see [CLA.md](CLA.md))

---

## Next Steps

1. Add camera capture → live video panel
2. Integrate MediaPipe hand/pose detection
3. Load ONNX model and run inference
4. Build ASL → Text → Speech pipeline
5. Add virtual camera/microphone output
6. Deploy backend API server

See [docs/02-GETTING-STARTED.md](docs/02-GETTING-STARTED.md) for the full roadmap.
