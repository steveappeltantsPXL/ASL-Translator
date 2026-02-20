# Getting Started

## Prerequisites

### All Platforms

| Tool          | Version | Purpose                             |
| ------------- | ------- | ----------------------------------- |
| Git           | 2.40+   | Version control                     |
| CMake         | 3.24+   | Build system                        |
| Ninja         | 1.11+   | Fast build execution                |
| vcpkg         | Latest  | C++ package manager                 |
| CLion         | 2024.1+ | IDE (or your preference)            |
| Python        | 3.11+   | ML training pipeline                |
| NVIDIA Driver | 535+    | GPU acceleration (NVIDIA GPUs only) |

### Windows

```powershell
# Install Visual Studio 2022 (Community, Pro, or Enterprise)
# Ensure "Desktop development with C++" workload is selected.
# This provides the MSVC compiler, debugger, and build tools.

# Install CMake
winget install Kitware.CMake

# Install Ninja
winget install Ninja-build.Ninja

# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
setx VCPKG_ROOT "C:\vcpkg"

# Install Python (for training pipeline)
winget install Python.Python.3.12

# Optional: Virtual Devices (for Zoom/Teams integration)
# Install OBS Studio for Virtual Camera
winget install obsproject.obs-studio
# Install VB-Audio Cable for Virtual Microphone
# Download from: https://vb-audio.com/Cable/
```

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew packages
brew install cmake ninja python@3.12

# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.zshrc
source ~/.zshrc
```

### Linux (Ubuntu 22.04+)

```bash
# System packages
sudo apt update && sudo apt install -y \
    build-essential cmake ninja-build git curl \
    pkg-config python3.12 python3.12-venv \
    libgl1-mesa-dev libglu1-mesa-dev \
    libx11-dev libxrandr-dev libxi-dev libxinerama-dev libxcursor-dev \
    libasound2-dev libpulse-dev \
    v4l-utils

# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc
source ~/.bashrc

# NVIDIA CUDA (if you have an NVIDIA GPU)
# Follow: https://developer.nvidia.com/cuda-downloads
```

---

## Project Setup

### Step 1: Clone and Initialize

```bash
git clone https://github.com/visear/asl-translator.git
cd asl-translator

# Initialize submodules (Dear ImGui, whisper.cpp, piper)
git submodule update --init --recursive
```

### Step 2: Vendor Dependencies

```bash
# Create vendor directory if not present
mkdir -p vendor

# Dear ImGui (docking branch for dockable panels)
git submodule add -b docking https://github.com/ocornut/imgui.git vendor/imgui

# whisper.cpp
git submodule add https://github.com/ggerganov/whisper.cpp.git vendor/whisper.cpp

# Piper TTS
git submodule add https://github.com/rhasspy/piper.git vendor/piper
```

### Step 3: Install vcpkg Dependencies

```bash
# vcpkg reads vcpkg.json from project root
$VCPKG_ROOT/vcpkg install
```

This installs: SDL3, OpenCV, ONNX Runtime (GPU), spdlog, nlohmann-json, SQLiteCpp, Google Test, cpr (HTTP client), and protobuf.

### Step 4: Download ML Models

```bash
# Download pre-trained models to resources/models/
./scripts/download-models.sh

# This downloads:
# - whisper-base.bin (~75 MB)
# - piper-en-us-lessac-medium.onnx (~30 MB)
# - (gesture model will come from your own training later)
```

For initial development, create a dummy gesture model:

```bash
cd training
python3 -m venv venv
source venv/bin/activate  # or venv\Scripts\activate on Windows
pip install -r requirements.txt

# Generate a small test model
python src/export_onnx.py --dummy \
    --output ../resources/models/gesture_classifier.onnx
```

### Step 5: Build

**Windows (Visual Studio 2022 — verified):**

```powershell
# Configure — use the VS generator with explicit x64 architecture
# Do NOT use -G Ninja on Windows; it defaults to x86 and breaks 64-bit packages
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows

# Build
cmake --build build --config Debug

# Run
.\build\Debug\VisearASLTranslator.exe
```

**macOS / Linux (Ninja):**

```bash
cmake -B build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build build
./build/VisearASLTranslator
```

---

## CLion First-Time Setup

### 1. Open Project

Open the project root directory in CLion. It will detect `CMakeLists.txt` and begin configuring.

### 2. Configure Toolchain

Go to **Settings → Build, Execution, Deployment → Toolchains**:

| Platform | Toolchain                          |
| -------- | ---------------------------------- |
| Windows  | Visual Studio 2022 (auto-detected) |
| macOS    | Clang (bundled with Xcode)         |
| Linux    | GCC or Clang (system)              |

### 3. Configure CMake

Go to **Settings → Build, Execution, Deployment → CMake**:

Add this to **CMake options**:

```
-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

Replace `/path/to/vcpkg` with your actual `$VCPKG_ROOT` path.

### 4. Select Build Target

In the top toolbar, select `VisearASLTranslator` as the build target.

### 5. Run

Press the green Run button (or Shift+F10). The application should launch with a Dear ImGui window.

---

## First Run Checklist

When the application launches for the first time, verify:

- [x] SDL window opens with ImGui rendering ✅ *Verified working*
- [x] FPS counter visible in status panel ✅ *Verified working*
- [ ] Camera panel shows live webcam feed *(not yet implemented)*
- [ ] Control panel shows mode dropdown *(not yet implemented)*
- [ ] No crash on startup (check console/logs)
- [ ] Settings can be changed and saved *(not yet implemented)*

### Minimal "Hello World" Main Loop

If you're starting from scratch, verify this minimal setup works before adding subsystems:

```cpp
// Minimal main.cpp to verify SDL + ImGui works
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <GL/gl.h>

int main(int, char**) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "Visear ASL Translator",
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    SDL_GLContext gl = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);  // VSync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 130");

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Demo window — remove once real panels are built
        ImGui::ShowDemoWindow();

        // Simple status
        ImGui::Begin("Visear ASL Translator");
        ImGui::Text("Application is running.");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();

        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
```

---

## Development Workflow

### Daily Development Cycle

```
1. Pull latest from develop branch
2. Open CLion, build (Ctrl+F9)
3. Run application (Shift+F10)
4. Make changes
5. Test: cmake --build build --target VisearTests && ./build/VisearTests
6. Commit to feature branch
7. Push and open PR against develop
```

### Adding a New Pipeline Stage

1. Create header and implementation in the appropriate `src/pipeline/` subdirectory
2. Add `#include` in `PipelineManager.h`
3. Wire into the relevant pipeline method (e.g., `runASLToText`)
4. Add unit test in `tests/`
5. Update CMakeLists.txt if new source files don't match the glob pattern

### Adding a New ImGui Panel

1. Create `src/ui/panels/NewPanel.h` and `.cpp`
2. Add a `render()` method that draws ImGui widgets
3. Call `newPanel.render()` from `UIManager::render()`
4. Add to the docking layout in UIManager

### Running the Training Pipeline

```bash
cd training
source venv/bin/activate

# Train a new model
python src/train.py --config config/gesture_model.yaml

# Export to ONNX
python src/export_onnx.py \
    --model experiments/latest/best_model.pt \
    --output ../resources/models/gesture_classifier.onnx

# Restart the desktop app — it loads the new model
```

---

## Troubleshooting

### Build Errors

**"vcpkg packages not found"**

- Verify `VCPKG_ROOT` is set: `echo $env:VCPKG_ROOT` (PowerShell)
- Verify the toolchain file path in CMake options is correct
- Run `C:\vcpkg\vcpkg install` manually from the project root

**"`builtin-baseline` is not a valid commit sha"**

- vcpkg requires the baseline to be a 40-character hex commit SHA, not a date
- Get the correct value: `git -C C:\vcpkg rev-parse HEAD`
- Update `vcpkg.json` with the result

**"`onnxruntime-gpu` only supported on ... not x86-windows"**

- You are using the Ninja generator which defaults to x86 on Windows
- Switch to: `cmake -B build -G "Visual Studio 17 2022" -A x64 ...`
- Always pass `-DVCPKG_TARGET_TRIPLET=x64-windows`

**"Could not find onnxruntimeConfig.cmake"**

- `onnxruntime-gpu` from vcpkg does not ship a CMake config file
- This is already handled in `CMakeLists.txt` via `find_path` / `find_library`
- If you see this error it means the vcpkg install hasn't run or the build dir was deleted

**"Cannot find source file: vendor/imgui/imgui.cpp"**

- The ImGui submodule hasn't been added yet
- Run: `git submodule add -b docking https://github.com/ocornut/imgui.git vendor/imgui`
- If `vendor/` is in `.gitignore`, remove that line first

**"The following paths are ignored by .gitignore: vendor"**

- Remove `vendor/` from `.gitignore` — submodules must be tracked by git
- The submodule contents themselves are not stored in your repo, only the commit ref

**"SDL3 not found"**

- SDL3 is in vcpkg stable and installs via `vcpkg.json` — ensure vcpkg install completed
- Verify: `ls build\vcpkg_installed\x64-windows\share\sdl3\`

**"MediaPipe not found"**

- MediaPipe is not yet integrated — it is commented out in `CMakeLists.txt`
- See `docs/03-ML-PIPELINE.md` for integration guidance when ready

**"ONNX Runtime GPU not available" at runtime**

- Ensure NVIDIA drivers are installed (check: `nvidia-smi`)
- The ONNX Runtime falls back to CPU automatically — this is expected behavior on non-NVIDIA hardware

### Runtime Issues

**Camera not detected**

- Check `cv::VideoCapture(0).isOpened()` — try device indices 0, 1, 2
- On Linux, verify permissions: `ls -la /dev/video*`
- On macOS, grant camera permission in System Preferences

**Low FPS**

- Check the debug panel for per-stage latency
- Ensure GPU execution provider is active (look for "CUDA" or "DirectML" in logs)
- Reduce camera resolution in config

**Virtual camera not visible in Teams/Zoom**

Recommended approach for Windows: use the **softcam** library (MIT license, C++), which creates a DirectShow virtual camera that appears in all applications.

### Windows Installation Steps

1.  **Install OBS Studio**: This is the most reliable way to get a signed, high-performance virtual camera driver on Windows.
    - Run `winget install obsproject.obs-studio` or download from [obsproject.com](https://obsproject.com/).
2.  **Enable Virtual Camera**: In OBS, click "Start Virtual Camera" (bottom right).
3.  **App Integration**: Our application will detect this driver and write to it.
4.  _(Alternative)_: For a standalone driver without OBS, install the [OBS-VirtualCam](https://github.com/Fenrirthviti/obs-virtual-cam) plugin.

- macOS: verify the DAL plugin is in `/Library/CoreMediaIO/Plug-Ins/DAL/`
- Linux: verify v4l2loopback is loaded: `lsmod | grep v4l2loopback`
- Restart the video conferencing app after enabling the virtual camera

---

## Next Steps

After completing the basic setup:

1. **Verify** the minimal ImGui window runs
2. **Add** OpenCV camera capture and display it in the camera panel
3. **Integrate** MediaPipe landmark extraction
4. **Load** a test ONNX model and run inference
5. **Connect** the pipeline stages end-to-end
6. **Add** virtual camera output
7. **Integrate** whisper.cpp for STT
8. **Integrate** Piper for TTS
9. **Build** the full mode selection UI
10. **Set up** the backend API server

Refer to each specific document for detailed guidance on each step.
