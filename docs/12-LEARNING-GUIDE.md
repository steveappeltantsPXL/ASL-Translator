# Visear ASL Translator — Learning Guide

> **Who this is for:** You are new to C++ and Dear ImGui and want to understand
> every decision made in this project — the tools, the build system, the code,
> and the architecture — so you can evaluate it and bring your own ideas.

---

## Table of Contents

1. [What This Project Does](#1-what-this-project-does)
2. [The Technology Stack — What Each Piece Is](#2-the-technology-stack--what-each-piece-is)
3. [The Build System — How the Project Is Compiled](#3-the-build-system--how-the-project-is-compiled)
4. [The Vendor Submodule — Dear ImGui](#4-the-vendor-submodule--dear-imgui)
5. [The Entry Point — src/main.cpp Line by Line](#5-the-entry-point--srcmaincpp-line-by-line)
6. [What Is Already Built vs What Is Planned](#6-what-is-already-built-vs-what-is-planned)
7. [The Planned Architecture](#7-the-planned-architecture)
8. [Where You Can Start Contributing](#8-where-you-can-start-contributing)
9. [Glossary](#9-glossary)

---

## 1. What This Project Does

Visear ASL Translator is a **real-time, bidirectional American Sign Language (ASL)
translation desktop application** written in C++.

"Bidirectional" means it handles both directions:

| Direction | Input | Output |
|-----------|-------|--------|
| ASL → Text/Speech | Webcam showing hand signs | Captions + spoken audio |
| Speech → ASL | Microphone audio | Animated avatar showing signs |

The goal is a tool that people who are deaf or hard-of-hearing can use in video
calls (Teams, Zoom, etc.) without the other party needing to do anything special.
The app injects its output into a **virtual camera** and **virtual microphone**,
so it looks like any regular webcam to the video call software.

**Current state of the project:**
The application window opens with a responsive UI layout (toolbar, camera feed placeholder, avatar panel, captions, controls) and a fully implemented 3D avatar renderer with skinned mesh animation.
The camera capture, sign recognition, speech-to-text, and virtual device output are designed and documented but not yet implemented. The avatar rendering pipeline (GLTF loading, skeleton, animation with crossfade blending) is complete.

---

## 2. The Technology Stack — What Each Piece Is

This section explains every technology used and *why* it was chosen.

### C++ (Language)

C++ was chosen because:
- Real-time computer vision and machine learning inference require **raw speed**
- Libraries like OpenCV and ONNX Runtime have their best APIs in C++
- The app runs 24/7 as a desktop process; C++ has the lowest overhead

If you are learning C++, the most important concepts you will encounter here:
- **Classes and objects** (for application, camera, pipeline, etc.)
- **Raw pointers vs smart pointers** (`SDL_Window*` vs `std::unique_ptr`)
- **Header files** (`.h`) declare what exists; **source files** (`.cpp`) define how it works
- **Linking** — code must be compiled into object files and then linked together

### SDL3 (Window and Input)

SDL stands for Simple DirectMedia Layer. SDL3 is its newest version.

SDL is responsible for:
1. Creating the OS window ("the black rectangle you see on screen")
2. Creating the OpenGL graphics context attached to that window
3. Delivering keyboard, mouse, and window events (resize, close, focus)
4. Handling VSync (preventing the screen from rendering faster than needed)

SDL does **not** draw UI. It only provides the surface and the events.

Think of SDL as the **foundation** that everything else sits on top of.

### OpenGL (Graphics API)

OpenGL is a standard graphics API. It talks to the GPU to draw things.

In this project, OpenGL does two things:
1. **Clears the screen** to a background color each frame
2. Renders the ImGui draw calls (ImGui generates geometry; OpenGL draws it)

Modern OpenGL 3.0+ is used here (not legacy "fixed-function" OpenGL).
You will not write raw OpenGL shader code to build the UI — Dear ImGui handles that.

### Dear ImGui (User Interface)

ImGui stands for "Immediate Mode GUI." This is the heart of the visible UI.

**Retained mode vs immediate mode** — traditional UI frameworks keep a tree of
widgets that exist permanently (retained). Dear ImGui works differently: every
single frame, your C++ code *calls functions to describe what should exist right
now*. ImGui builds the draw list and throws it away, ready for the next frame.

```
Traditional (retained):
  Create button once → button lives in memory → events fire when clicked

ImGui (immediate):
  Every frame: if (ImGui::Button("Click me")) { handle_click(); }
  The button only exists as draw commands during that frame
```

The advantage: the UI is just code. There is no XML, no designer, no
widget tree to maintain. The disadvantage: it can be harder to build very complex
or animation-heavy UIs.

ImGui is widely used in game development, tools, debuggers, and scientific
visualization — exactly the kind of real-time, performance-sensitive tool this is.

**The docking branch** is a fork of the main ImGui repo that adds the ability to
dock panels (drag them to edges of the window, snap them together). This project
uses the docking branch because the planned UI has multiple panels
(camera feed, captions, controls) that users should be able to rearrange.

### OpenCV (Computer Vision)

OpenCV is the most widely used open-source computer vision library.

In this project it will be used for:
- Reading frames from the webcam (`cv::VideoCapture`)
- Resizing and normalizing frames before feeding them to the neural network
- Drawing landmark overlays on the camera feed in the UI

OpenCV is installed via vcpkg and its include/lib files are in `vcpkg_installed/`.
It is linked to the executable but not yet called from any code.

### ONNX Runtime (Machine Learning Inference)

ONNX (Open Neural Network Exchange) is a standard format for trained AI models.
A model trained in Python (PyTorch, TensorFlow) can be exported to `.onnx` format
and then loaded in C++ without needing Python at runtime.

ONNX Runtime is a C++ library that loads `.onnx` files and runs inference
(makes predictions). The "GPU" variant uses CUDA or DirectML to run inference
on the GPU, which is required for real-time performance.

This project will use ONNX Runtime to:
- Detect hand and body landmarks from camera frames
- Classify the sequence of landmarks into ASL gestures
- Map gesture sequences to words and sentences

ONNX Runtime is unusual because it ships no CMake config file. This is why
`CMakeLists.txt` uses `find_path()` and `find_library()` instead of
`find_package()` to locate it.

### spdlog (Logging)

spdlog is a fast C++ logging library. It replaces `std::cout` with structured
log output that includes timestamps, log levels (debug, info, warn, error),
and can write to files and consoles simultaneously.

You will use it like this:
```cpp
#include <spdlog/spdlog.h>
spdlog::info("Camera opened: {}x{}", width, height);
spdlog::error("Failed to load model: {}", model_path);
```

### nlohmann/json (JSON Parsing)

This is the most popular C++ JSON library. It lets you read and write JSON
config files cleanly:
```cpp
#include <nlohmann/json.hpp>
auto config = nlohmann::json::parse(file);
std::string model_path = config["model"]["path"];
```

It will be used to read application configuration files.

### SQLiteCpp (Database)

SQLite is a lightweight, file-based SQL database — no separate server required.
SQLiteCpp is a C++ wrapper around the SQLite C library.

This project will use it to store:
- The ASL dictionary (sign → word mappings)
- Usage analytics
- User preferences

### GTest (Testing)

Google Test is the standard C++ unit testing framework. It is installed but
not yet wired into the build. As features are implemented, unit tests should
be added in a `tests/` directory.

### cpr (HTTP Client)

cpr is a C++ library for making HTTP requests. It will be used when the app
needs to talk to the backend API server (model updates, analytics, cloud fallback).

### protobuf (Serialization)

Protocol Buffers (protobuf) is Google's format for serializing structured data
efficiently. It is a dependency of ONNX Runtime and may also be used for the
API communication layer.

---

## 3. The Build System — How the Project Is Compiled

Understanding the build system is essential before you can write any code.

### The Problem Build Systems Solve

You cannot just type `g++ main.cpp` for a project with 9 libraries and dozens
of source files. A build system answers three questions:
1. What files need to be compiled?
2. What compiler flags should be used?
3. What libraries need to be linked?

### CMake

CMake is a **build system generator**. It reads `CMakeLists.txt` and writes
build files for your specific environment. On Windows with Visual Studio, it
generates a `.sln` and `.vcxproj` files. On Linux with Make, it generates
`Makefile`s.

The key file is `CMakeLists.txt` in the project root. You can think of it as
a recipe: "to build this project, find these libraries, compile these files,
link them together like this."

**Important CMake concepts you will see:**

```cmake
# Set a variable
set(MY_VAR "some value")

# Conditional
if(WIN32)
    # only on Windows
endif()

# Find an installed package (looks in CMAKE_PREFIX_PATH)
find_package(SDL3 CONFIG REQUIRED)

# Create an executable from source files
add_executable(MyApp src/main.cpp src/other.cpp)

# Link libraries to the executable
target_link_libraries(MyApp PRIVATE SDL3::SDL3 spdlog::spdlog)

# Add include directories (so #include works)
target_include_directories(MyApp PRIVATE src/)
```

### vcpkg

vcpkg is a **C++ package manager** (like npm for JavaScript or pip for Python).
It downloads, compiles, and installs C++ libraries.

This project uses vcpkg in **manifest mode**: the file `vcpkg.json` lists all
required packages (like `package.json` in Node.js). When CMake runs, vcpkg
automatically installs any missing packages.

The installed files end up in `cmake-build-debug-msvc/vcpkg_installed/x64-windows/`:
- `include/` — header files (`.h`, `.hpp`)
- `lib/` — library files (`.lib` on Windows, `.a` on Linux)
- `bin/` — DLL files (`.dll`) needed at runtime
- `share/*/` — CMake config files that `find_package()` reads

**Triplet:** `x64-windows` is the "triplet" — a shorthand for the target
platform (64-bit Windows). Everything is compiled for this target.

### Why vcpkg_installed Is in the Build Directory

Normally, vcpkg installs to a `vcpkg_installed` folder next to `vcpkg.json`
(the source directory). CLion places it inside the build directory instead.
This is why `CMakeLists.txt` explicitly adds the build directory path to
`CMAKE_PREFIX_PATH` — so `find_package()` can locate the installed packages.

### MSVC Runtime Configuration

You may see `/MD` and `/MT` flags mentioned in build errors. These control which
C++ runtime the compiled code links against:
- `/MD` — Dynamic: links `VCRUNTIME140.dll` at runtime (smaller binary)
- `/MT` — Static: bakes the runtime into the `.exe` (larger, more portable)

This project uses `MultiThreadedDLL` (`/MD`) because vcpkg's default packages
are compiled with `/MD`. Mixing `/MD` and `/MT` in the same binary causes
crashes, so everything must be consistent.

---

## 4. The Vendor Submodule — Dear ImGui

### What a Git Submodule Is

A git submodule is a pointer from one git repository to a specific commit in
another repository. Instead of copying ImGui's source files into this project
(which would make updating harder), the project records:
- "There is a folder `vendor/imgui`"
- "It should contain commit `ad769352e` from the ImGui docking repository"

When you clone the project and run `git submodule update --init --recursive`,
git fetches that exact commit and places it in `vendor/imgui/`.

### Was Any Code Modified in the Submodule?

**No.** `vendor/imgui` is a clean, unmodified import.

The git log of `vendor/imgui` shows only upstream Dear ImGui commits
(the most recent is v1.92.6-docking-1). No project-specific changes were made.

This is the correct approach. You should **never modify files inside a submodule**
unless you own that submodule. If ImGui needs a patch, the right way is to fork
the ImGui repository, apply the patch there, and point the submodule at your fork.

### How ImGui Is Compiled

ImGui ships as source code only — there is no pre-compiled binary. It must be
compiled as part of this project.

`CMakeLists.txt` does this by adding ImGui's source files to a static library:

```cmake
add_library(imgui STATIC
    vendor/imgui/imgui.cpp          # Core ImGui
    vendor/imgui/imgui_draw.cpp     # Drawing (shapes, text)
    vendor/imgui/imgui_tables.cpp   # Table widget
    vendor/imgui/imgui_widgets.cpp  # Buttons, sliders, etc.
    vendor/imgui/imgui_demo.cpp     # Demo window (optional)
    vendor/imgui/backends/imgui_impl_sdl3.cpp      # SDL3 integration
    vendor/imgui/backends/imgui_impl_opengl3.cpp   # OpenGL3 rendering
)
```

The two backend files are the "glue" between ImGui and the platform:
- `imgui_impl_sdl3.cpp` — translates SDL3 events (mouse, keyboard) into ImGui input
- `imgui_impl_opengl3.cpp` — takes ImGui's draw list and renders it with OpenGL

This static library (`imgui.lib`) is then linked into the main executable.

---

## 5. The Entry Point — src/main.cpp Line by Line

This is the **entry point** of the project. The `src/avatar/` directory contains
additional source files implementing the 3D avatar renderer. The main file is
produces the window you see when you run the app. Every line is explained below.

```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
```

These `#include` lines pull in declarations from header files.
- `SDL3/SDL.h` — all SDL3 functions and types
- `SDL3/SDL_opengl.h` — SDL's wrapper for OpenGL (sets up OpenGL types)
- `imgui.h` — ImGui functions (`ImGui::Begin`, `ImGui::Button`, etc.)
- `imgui_impl_sdl3.h` — the SDL3 backend for ImGui
- `imgui_impl_opengl3.h` — the OpenGL3 backend for ImGui

---

```cpp
int main(int argc, char* argv[])
{
```

Every C++ program starts at `main`. On Windows with SDL3, `main` must take
`(int argc, char* argv[])` — SDL3 may redefine `main` internally to
`SDL_main` for platform reasons.

---

```cpp
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return 1;
    }
```

`SDL_Init` starts SDL with the video subsystem. If it fails (returns false),
the program exits with code 1 (error). No window or anything exists yet.

---

```cpp
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
```

These lines configure what kind of OpenGL context to create, **before** the
window is made:
- Core profile (no deprecated legacy functions)
- OpenGL 3.0 minimum
- Double-buffered (one buffer is shown while the other is drawn to; they swap each frame)
- 24-bit depth buffer and 8-bit stencil buffer (used by 3D rendering; ImGui doesn't need them but they're good practice)

---

```cpp
    SDL_Window* window = SDL_CreateWindow(
        "Visear ASL Translator",
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!window) { SDL_Quit(); return 1; }
```

This creates the OS window. `SDL_Window*` is a raw pointer to an opaque SDL
structure. The `|` operator combines flags (bitwise OR). If window creation
fails, clean up SDL and exit.

---

```cpp
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // VSync on
```

`SDL_GL_CreateContext` attaches an OpenGL context to the window.
`SDL_GL_MakeCurrent` makes this context active for the current thread
(OpenGL is thread-local).
`SDL_GL_SetSwapInterval(1)` enables VSync — the swap waits for the monitor's
refresh signal. This caps the frame rate at the monitor's refresh rate and
prevents the GPU from running at 100% unnecessarily.

---

```cpp
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
```

ImGui initialization:
- `IMGUI_CHECKVERSION()` — a macro that verifies the ImGui header and library
  versions match (catches subtle ABI mismatches)
- `ImGui::CreateContext()` — allocates ImGui's internal state
- `ImGuiIO& io` — the "IO" struct is ImGui's configuration and input hub.
  It is a reference (`&`), not a copy, so changes apply globally.
- `ImGuiConfigFlags_DockingEnable` — the `|=` operator sets this bit flag,
  enabling the docking feature from the docking branch
- `StyleColorsDark()` — applies the dark color theme

---

```cpp
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330 core");
```

Backend initialization:
- `ImGui_ImplSDL3_InitForOpenGL` — tells ImGui's SDL3 backend which window
  and GL context to use for reading input
- `ImGui_ImplOpenGL3_Init("#version 330 core")` — tells the OpenGL backend what
  GLSL (shader language) version to use. Version 330 core corresponds to OpenGL 3.3.

---

```cpp
    bool running = true;
    while (running)
    {
```

The **main loop**. This runs approximately 60 times per second (limited by VSync).
Every iteration = one frame.

---

```cpp
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }
```

`SDL_PollEvent` retrieves one event from the queue at a time (non-blocking).
The inner `while` drains all queued events.
- `ImGui_ImplSDL3_ProcessEvent(&event)` — forwards each event to ImGui so it
  knows about mouse clicks, key presses, etc.
- `SDL_EVENT_QUIT` fires when the user clicks the window's X button.

---

```cpp
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
```

Start a new ImGui frame. These must be called in this order every frame before
any `ImGui::*` UI calls. They reset internal state and prepare ImGui to collect
draw commands.

---

```cpp
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags dockspace_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove     |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        ImGui::Begin("DockSpace", nullptr, dockspace_flags);
        ImGui::DockSpace(ImGui::GetID("MainDockSpace"));
        ImGui::End();
```

This creates the **dockspace** — an invisible full-window area that other
ImGui windows can dock onto (snap to edges or snap next to each other).

`ImGuiWindowFlags_*` flags configure the container window:
- No title bar, no collapse button, not resizable, not movable
- It is pinned exactly to the OS window bounds
- `NoBringToFrontOnFocus` / `NoNavFocus` prevent it from intercepting clicks
  meant for the docked children

`ImGui::DockSpace()` registers the docking area with an ID so child windows
can refer to it.

---

```cpp
        ImGui::Begin("Status");
        ImGui::Text("Status: Running");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
```

A simple status panel. This is the only visible UI element right now.
- `ImGui::Begin("Status")` — opens a window titled "Status" (you see it in the top-left)
- `ImGui::Text(...)` — renders a line of text. `%.1f` is a C printf-style
  format specifier (float with 1 decimal place)
- `ImGui::GetIO().Framerate` — ImGui calculates a smoothed frame rate from
  recent frame times
- `ImGui::End()` — closes the window. Every `Begin` must have a matching `End`.

---

```cpp
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
```

Rendering:
- `ImGui::Render()` — finalizes ImGui's draw list (geometry for this frame)
- `glViewport(...)` — tells OpenGL the pixel area to render into
- `glClearColor(...)` — sets the background color (dark gray; values are 0.0–1.0)
- `glClear(GL_COLOR_BUFFER_BIT)` — fills the screen with the clear color
- `ImGui_ImplOpenGL3_RenderDrawData(...)` — sends ImGui's draw commands to OpenGL
- `SDL_GL_SwapWindow(window)` — swaps front/back buffers; the newly drawn frame
  appears on screen, the old one becomes the next frame's back buffer

---

```cpp
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

Cleanup in reverse initialization order:
1. Shut down ImGui backends
2. Destroy ImGui context (frees all ImGui memory)
3. Delete OpenGL context
4. Destroy the SDL window
5. Quit SDL entirely
6. Return 0 (success)

**Always clean up in reverse order.** Resources that depend on others must be
released before the things they depend on.

---

## 6. What Is Already Built vs What Is Planned

This table gives you an honest snapshot of the project right now.

| Component | Status | Notes |
|-----------|--------|-------|
| CMake build system | ✅ Complete | Works on Windows x64, all packages found |
| vcpkg dependencies | ✅ Installed | 9 packages, all in build dir |
| SDL3 window + GL context | ✅ Working | In `main.cpp` |
| Dear ImGui responsive layout | ✅ Working | Desktop + mobile modes in `main.cpp` |
| Toolbar + Controls panels | ✅ Working | In `main.cpp` |
| Dear ImGui vendor submodule | ✅ Clean import | No modifications made |
| 3D Avatar renderer | ✅ Working | Skinned mesh, skeleton, animation blending in `src/avatar/` |
| Documentation (12 guides) | ✅ Complete | Architecture, ML, API, mobile all designed |
| GitHub templates & workflow | ✅ Complete | PR/issue templates, branch strategy |
| Application class | ❌ Not written | Planned: wraps the SDL/GL/ImGui lifecycle |
| UIManager class | ❌ Not written | Planned: manages multiple ImGui panels |
| Camera capture (OpenCV) | ❌ Not written | OpenCV installed, no code written |
| Camera panel (ImGui texture) | ❌ Not written | Displaying a video frame in ImGui |
| MediaPipe hand landmarks | ❌ Not written | Submodule not added yet |
| ONNX inference session | ❌ Not written | Runtime installed, no session code |
| ASL gesture classifier | ❌ Not written | Model not trained yet |
| Caption panel | ❌ Not written | Display recognized text |
| Control panel | ❌ Not written | Mode selection, settings |
| whisper.cpp (speech-to-text) | ❌ Not started | Submodule not added |
| Piper (text-to-speech) | ❌ Not started | Submodule not added |
| Virtual camera output | ❌ Not written | Platform-specific DirectShow/v4l2 code |
| Virtual microphone output | ❌ Not written | Platform-specific audio driver |
| SQLite dictionary | ❌ Not written | Library installed, no schema defined |
| Backend API (FastAPI) | ❌ Not written | Python server, fully designed in docs |
| Unit tests | ❌ Not written | GTest installed, no test files |

**Summary:** The foundation (build, window, UI framework) is solid and verified.
The feature code has not been started. You are at the best possible entry point.

---

## 7. The Planned Architecture

The full architecture is described in `docs/04-ARCHITECTURE-OVERVIEW.md` and
`docs/05-PROJECT-STRUCTURE.md`. Here is a condensed view.

### Data Flow (ASL → Text direction)

```
Webcam
  │
  ▼
CameraCapture (OpenCV cv::VideoCapture)
  │  raw frames (BGR)
  ▼
MediaPipe (hand/body landmark detection)
  │  225-dimensional feature vector per frame
  ▼
ONNXRuntime (gesture classifier model)
  │  gesture probabilities
  ▼
GestureBuffer (sliding window, sequence smoothing)
  │  recognized gesture or word
  ▼
NLP Correction (grammar, context)
  │  cleaned sentence
  ▼
CaptionPanel (ImGui Text)        →  displayed in UI
VirtualCamera (DirectShow/v4l2)  →  injected into video call
TTS (Piper)                      →  spoken audio output
VirtualMicrophone                →  injected into video call
```

### Planned Source Directory Structure

```
src/
├── main.cpp                    ← exists now
├── app/
│   ├── Application.h/.cpp      ← owns SDL window, GL context, main loop
│   └── Config.h/.cpp           ← JSON config loading
├── ui/
│   ├── UIManager.h/.cpp        ← owns all ImGui panels
│   ├── CameraPanel.h/.cpp      ← shows live camera frame as ImGui texture
│   ├── CaptionPanel.h/.cpp     ← shows recognized text
│   ├── ControlPanel.h/.cpp     ← mode toggles, settings
│   ├── DictionaryPanel.h/.cpp  ← browse ASL dictionary
│   └── DebugPanel.h/.cpp       ← FPS, latency, model stats
├── capture/
│   ├── CameraCapture.h/.cpp    ← OpenCV VideoCapture wrapper
│   └── AudioCapture.h/.cpp     ← microphone input
├── pipeline/
│   ├── PipelineManager.h/.cpp  ← orchestrates all stages
│   ├── ASLRecognizer.h/.cpp    ← landmark → gesture → word
│   └── SpeechRecognizer.h/.cpp ← audio → text (whisper.cpp)
├── ml/
│   ├── ONNXSession.h/.cpp      ← ONNX Runtime wrapper
│   └── ModelManager.h/.cpp     ← load/swap models at runtime
├── output/
│   ├── VirtualCamera.h/.cpp    ← write frames to virtual camera driver
│   └── CaptionOverlay.h/.cpp   ← burn captions onto output frame
└── utils/
    ├── Logger.h                ← spdlog wrapper
    ├── ThreadPool.h/.cpp       ← background thread management
    └── RingBuffer.h            ← lock-free frame queue
```

### Threading Model

The main loop runs on the main thread (required for SDL and ImGui). Heavy work
runs on background threads:

```
Main thread:       SDL events → ImGui render → screen output
Camera thread:     webcam read → frame queue
Pipeline thread:   frame queue → landmarks → gesture → text
Audio thread:      microphone read → whisper inference
```

A `RingBuffer` (lock-free circular buffer) will pass frames between threads
without blocking the UI.

---

## 8. Where You Can Start Contributing

These are the best starting points, roughly ordered from easiest to hardest.

### Beginner: Add a new ImGui panel

Open `src/main.cpp`. After the Status panel, add a new panel:

```cpp
ImGui::Begin("Hello World");
ImGui::Text("This is my first ImGui panel!");
if (ImGui::Button("Click me"))
{
    // This code runs only when the button is clicked
    spdlog::info("Button was clicked");
}
ImGui::End();
```

Run the app and you will see your new panel. Try adding:
- A slider: `ImGui::SliderFloat("Size", &myValue, 0.0f, 100.0f);`
- A checkbox: `ImGui::Checkbox("Enable feature", &myBool);`
- A color picker: `ImGui::ColorEdit3("Color", myColor);`

The Dear ImGui demo window (shipped in `vendor/imgui/imgui_demo.cpp`) shows
every widget with live code examples. Add this to the main loop to open it:
```cpp
ImGui::ShowDemoWindow();
```

### Intermediate: Create the Application class

Refactor `main.cpp` into a proper class. Create `src/app/Application.h`:

```cpp
class Application {
public:
    Application();
    ~Application();
    bool init();
    void run();
private:
    void process_events();
    void render();
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_gl_context = nullptr;
    bool m_running = false;
};
```

Move the SDL/ImGui init code into `Application::init()`, the main loop into
`Application::run()`, and the cleanup into `~Application()` (destructor).
Then `main.cpp` becomes simply:

```cpp
int main(int argc, char* argv[]) {
    Application app;
    if (!app.init()) return 1;
    app.run();
    return 0;
}
```

### Intermediate: Display a camera frame

Add OpenCV camera capture and show the frame as an ImGui image:

```cpp
// One-time setup
cv::VideoCapture cap(0); // camera index 0
GLuint texture_id;
glGenTextures(1, &texture_id);

// In the render loop
cv::Mat frame;
cap.read(frame);
cv::cvtColor(frame, frame, cv::COLOR_BGR2RGBA);

glBindTexture(GL_TEXTURE_2D, texture_id);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame.cols, frame.rows,
             0, GL_RGBA, GL_UNSIGNED_BYTE, frame.data);

// In ImGui
ImGui::Begin("Camera");
ImGui::Image((ImTextureID)(intptr_t)texture_id,
             ImVec2(640, 480));
ImGui::End();
```

### Advanced: Load and run an ONNX model

```cpp
#include <onnxruntime_cxx_api.h>

Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "VisearASL");
Ort::Session session(env, L"model.onnx", Ort::SessionOptions{});

// Prepare input tensor
std::vector<float> input_data(225); // 225-dim feature vector
// ... fill input_data from landmarks ...

// Run inference
auto output_tensors = session.Run(...);
```

---

## 9. Glossary

| Term | Meaning |
|------|---------|
| **ABI** | Application Binary Interface — the low-level contract that compiled code uses to call functions. ABI mismatches cause crashes. |
| **Backend (ImGui)** | Platform-specific code that bridges ImGui to SDL/OpenGL. |
| **CMake** | Tool that generates build files (Visual Studio .sln, Makefiles) from `CMakeLists.txt`. |
| **Docking (ImGui)** | Feature that allows ImGui windows to be pinned to edges of the main window. |
| **Double buffering** | Two framebuffers: one displayed, one being drawn. They swap each frame to avoid tearing. |
| **DLL** | Dynamic-Link Library — Windows shared library (`.dll`) loaded at runtime. |
| **Frame** | One complete rendered image. At 60 FPS, there are 60 frames per second. |
| **Framebuffer** | Memory holding the color values of every pixel to be displayed. |
| **Git submodule** | A nested git repository tracked by a parent repository. |
| **ImGuiIO** | Dear ImGui's central configuration and input state struct. |
| **Immediate mode** | UI paradigm where widgets are drawn (and forgotten) every frame from code. |
| **Latency budget** | The maximum allowed delay. For real-time translation, <100ms total. |
| **Linker** | Combines compiled `.obj` files and `.lib` libraries into the final `.exe`. |
| **MSVC** | Microsoft Visual C++ — the C++ compiler that ships with Visual Studio. |
| **ONNX** | Open Neural Network Exchange — standard format for trained AI models. |
| **OpenGL** | Cross-platform graphics API for rendering 2D/3D graphics using the GPU. |
| **RingBuffer** | Circular buffer for passing data between threads without locks. |
| **SDL3** | Simple DirectMedia Layer v3 — handles windows, input, and OpenGL contexts. |
| **Static library** | Pre-compiled code bundled directly into the executable (`.lib` on Windows). |
| **Triplet (vcpkg)** | Target platform identifier, e.g. `x64-windows` (64-bit Windows). |
| **VSync** | Vertical sync — GPU waits for monitor refresh before swapping buffers. |
| **vcpkg** | C++ package manager. Downloads and compiles libraries declared in `vcpkg.json`. |
| **vcpkg manifest mode** | Mode where `vcpkg.json` in the project root lists all packages. vcpkg auto-installs them during CMake configure. |

---

*This document describes the project as of February 2026. As features are
implemented, the source directory will grow. Keep this document updated by
adding entries to sections 6 and 7 as new components are built.*
