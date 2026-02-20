# ASL Translator

A real-time American Sign Language (ASL) recognition and translation desktop application built with Dear ImGui, SDL2, and ONNX Runtime, targeting Windows with MSVC.

---

## Project Overview

This application captures live webcam video, processes hand/gesture landmarks using a computer vision pipeline (MediaPipe or equivalent), and runs inference via a pre-trained ONNX model to translate ASL gestures into text in real time. The UI is rendered using Dear ImGui over an SDL2 window with a DirectX 11 backend.

---

## Technology Stack

| Component       | Technology                                   |
| --------------- | -------------------------------------------- |
| UI Framework    | Dear ImGui (Docking branch)                  |
| Windowing/Input | SDL2                                         |
| Renderer        | DirectX 11 (via ImGui DX11 backend)          |
| ML Inference    | ONNX Runtime (C++ API)                       |
| Computer Vision | OpenCV (webcam capture + preprocessing)      |
| Language        | C++17                                        |
| Build/IDE       | Visual Studio (MSVC), Empty Project template |

---

## Visual Studio Project Setup

### Template

Use the **Empty Project** template in Visual Studio:

> File → New → Project → C++ → General → **Empty Project**

Do **not** use the Windows Desktop Application wizard. SDL2 manages the entry point (`SDL_main`), and using a wizard-generated `WinMain` will conflict.

### Key Project Properties

Navigate to **Project → Properties** and configure:

- **Configuration Properties → General**
  - C++ Language Standard: `ISO C++17 (/std:c++17)`

- **Linker → System**
  - SubSystem: `Windows (/SUBSYSTEM:WINDOWS)`

- **VC++ Directories**
  - Include Directories: paths to SDL2, ImGui, ONNX Runtime, and OpenCV headers
  - Library Directories: paths to their respective `.lib` files

- **Linker → Input → Additional Dependencies**

  ```
  SDL2.lib
  SDL2main.lib
  d3d11.lib
  d3dcompiler.lib
  onnxruntime.lib
  opencv_world4xx.lib
  ```

- **Build Events → Post-Build**
  - Copy required `.dll` files (`SDL2.dll`, `onnxruntime.dll`, `opencv_worldXXX.dll`) to the output directory.

---

## Repository Structure (Planned)

```
asl-translator/
├── src/
│   ├── main.cpp                  # Entry point, SDL2 + ImGui init
│   ├── ui/
│   │   ├── AppWindow.h/.cpp      # Main application window layout
│   │   ├── VideoPanel.h/.cpp     # Live webcam feed panel
│   │   └── TranslationPanel.h/.cpp  # Output/history display
│   ├── capture/
│   │   ├── CameraCapture.h/.cpp  # OpenCV webcam wrapper
│   │   └── FrameProcessor.h/.cpp # Preprocessing pipeline
│   ├── inference/
│   │   ├── OnnxModel.h/.cpp      # ONNX Runtime wrapper
│   │   └── GestureClassifier.h/.cpp  # Pre/post-processing for model I/O
│   └── utils/
│       ├── Logger.h/.cpp
│       └── Config.h/.cpp
├── assets/
│   └── models/
│       └── asl_model.onnx        # Trained ONNX model (not included, see below)
├── third_party/
│   ├── imgui/                    # Dear ImGui source (vendored)
│   ├── SDL2/                     # SDL2 headers + libs
│   ├── onnxruntime/              # ONNX Runtime headers + libs
│   └── opencv/                   # OpenCV headers + libs
├── docs/
│   └── architecture.md
├── .gitignore
└── README.md
```

---

## Dependencies

### Acquiring Dependencies

| Library      | Source                                              | Notes                              |
| ------------ | --------------------------------------------------- | ---------------------------------- |
| Dear ImGui   | https://github.com/ocornut/imgui (`docking` branch) | Vendor into `third_party/imgui/`   |
| SDL2         | https://github.com/libsdl-org/SDL/releases          | Use VC dev package (`.lib`+`.dll`) |
| ONNX Runtime | https://github.com/microsoft/onnxruntime/releases   | Use `onnxruntime-win-x64` package  |
| OpenCV       | https://opencv.org/releases/                        | Prebuilt Windows binary            |

All third-party libraries should be placed under `third_party/` and are excluded from version control (see `.gitignore`).

---

## UI Layout (Target)

The application UI consists of two primary panels side-by-side within a dockable ImGui layout:

```
┌──────────────────────────────────────────────────┐
│  ASL Translator                          [≡] [x] │
├────────────────────┬─────────────────────────────┤
│                    │  Translation Output         │
│   Live Camera      │ ─────────────────────────── │
│   Feed             │  Detected: [HELLO]          │
│   (OpenCV → ImGui  │                             │
│    texture)        │  History:                   │
│                    │  > HELLO                    │
│                    │  > MY NAME IS               │
├────────────────────┴─────────────────────────────┤
│  Status: Running | FPS: 30 | Model: asl_v1.onnx  │
└──────────────────────────────────────────────────┘
```

### UI Components to Implement

- **VideoPanel** — renders an OpenCV frame as an `ImGui::Image()` via a DirectX 11 shader resource view updated each frame
- **TranslationPanel** — scrollable output log with current detected gesture highlighted
- **StatusBar** — inference latency, FPS counter, model name, and camera index
- **Settings Modal** — camera device selection, confidence threshold slider, model file path picker

---

## Model

The ONNX model is trained separately (Python/PyTorch or TensorFlow) and exported to ONNX format. It is not included in this repository.

Expected model I/O contract:

| Property     | Value                                      |
| ------------ | ------------------------------------------ |
| Input name   | `input`                                    |
| Input shape  | `[1, N, 3]` — N landmarks × (x, y, z)      |
| Output name  | `output`                                   |
| Output shape | `[1, num_classes]` — softmax probabilities |

Place the model at `assets/models/asl_model.onnx` or configure the path at runtime via the Settings panel.

---

## Build Instructions

1. Clone the repository and populate `third_party/` with the required libraries.
2. Open `asl-translator.sln` in Visual Studio 2022 (or later).
3. Select **x64 | Release** (or Debug for development).
4. Build → **Build Solution** (`Ctrl+Shift+B`).
5. Ensure all required `.dll` files are present in the output directory (post-build event handles this if configured).

---

## Roadmap

- [ ] Project scaffolding and MSVC build configuration
- [ ] SDL2 + ImGui + DX11 window initialization
- [ ] OpenCV webcam capture and frame-to-texture pipeline
- [ ] ONNX Runtime model loading and inference wrapper
- [ ] Gesture classification and translation output
- [ ] Dockable UI layout with VideoPanel and TranslationPanel
- [ ] Settings and configuration persistence
- [ ] Model hot-reload support
- [ ] Sentence-level buffering and word boundary detection
- [ ] Export/copy translation output

---

## License

TBD

---

## Contributing

TBD
