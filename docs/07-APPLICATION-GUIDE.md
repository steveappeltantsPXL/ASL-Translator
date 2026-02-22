# Desktop Application Guide

## Application Lifecycle

```
main()
  │
  ▼
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)
  │
  ▼
Create SDL Window + OpenGL 3.3 Core Context
  │
  ▼
Initialize Dear ImGui (SDL3 + OpenGL3 backend, GLSL #version 330 core)
  │
  ▼
Initialize AvatarRenderer (GLEW + FBO + GLTF model + shaders)
  │
  ▼
Initialize subsystems (planned):
  ├── CameraCapture (OpenCV)
  ├── AudioCapture (SDL_Audio)
  ├── ONNXRuntime (load models)
  ├── MediaPipe (landmark extractor → feeds AvatarRenderer::setPose())
  ├── WhisperEngine (STT)
  ├── TTSEngine (Piper)
  ├── VirtualCamera (platform-specific)
  ├── VirtualMicrophone (platform-specific)
  └── PipelineManager (orchestrate everything)
  │
  ▼
Main Loop:
  ┌──────────────────────────────────────────────┐
  │  1. SDL_PollEvent (input handling)           │
  │  2. PipelineManager.tick()                   │
  │     └── runs active translation pipeline     │
  │  3. AvatarRenderer::render(w, h, dt)         │  ← renders to FBO before ImGui
  │  4. ImGui::NewFrame()                        │
  │  5. Render UI panels                         │
  │     └── "ASL Avatar": ImGui::Image(FBO tex)  │
  │  6. ImGui::Render()                          │
  │  7. SDL_GL_SwapWindow()                      │
  └──────────────────────────────────────────────┘
  │
  ▼ (on quit)
avatarRenderer.shutdown()   ← must happen while GL context is current
ImGui_ImplOpenGL3_Shutdown()
SDL_GL_DestroyContext()
SDL_Quit()
```

---

## Main Entry Point (current)

The entire application currently lives in `src/main.cpp`. The planned `Application`
class refactor is a Phase 2 task. The avatar system (`src/avatar/`) is the first
extracted subsystem.

```cpp
// src/main.cpp — current structure (abbreviated)
#include "avatar/AvatarRenderer.h"

int main(int, char**) {
    SDL_Init(SDL_INIT_VIDEO);

    // GL 3.3 core — required for GLSL 3.30 (avatar shaders)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Visear Translator", 1280, 720,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl = SDL_GL_CreateContext(window);

    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Avatar renderer — must init after GL context is current
    avatar::AvatarRenderer avatarRenderer;
    bool avatarOk = avatarRenderer.init("resources/models/avatar/avatar.glb");

    while (running) {
        // 1. Avatar renders to its FBO before ImGui starts the frame
        avatarRenderer.render(panelW, panelH, io.DeltaTime);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // 2. Display FBO texture in the avatar panel
        ImGui::Begin("ASL Avatar", nullptr, locked);
        ImGui::Image(avatarRenderer.getTexture(),
                     ImGui::GetContentRegionAvail(),
                     ImVec2(0, 1), ImVec2(1, 0));  // UV flip: GL y-origin
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    avatarRenderer.shutdown();  // GL resources — before SDL_GL_DestroyContext
    ImGui_ImplOpenGL3_Shutdown();
    SDL_GL_DestroyContext(gl);
}
```

---

## Planned Application Core

```cpp
// src/app/Application.h (Phase 2)
#pragma once
#include <SDL3/SDL.h>
#include "ui/UIManager.h"
#include "pipeline/PipelineManager.h"
#include "capture/CameraCapture.h"
#include "capture/AudioCapture.h"
#include "ml/ModelManager.h"
#include "output/VirtualCamera.h"
#include "avatar/AvatarRenderer.h"
#include "Config.h"
#include <memory>

class Application {
public:
    bool initialize(int argc, char* argv[]);
    void run();
    void shutdown();

private:
    SDL_Window*    window_     = nullptr;
    SDL_GLContext  gl_context_ = nullptr;

    std::unique_ptr<Config>          config_;
    std::unique_ptr<ModelManager>    models_;
    std::unique_ptr<CameraCapture>   camera_;
    std::unique_ptr<AudioCapture>    audio_;
    std::unique_ptr<PipelineManager> pipeline_;
    std::unique_ptr<VirtualCamera>   vcam_;
    std::unique_ptr<UIManager>       ui_;
    avatar::AvatarRenderer           avatarRenderer_;  // already implemented

    bool running_ = false;

    bool initSDL();
    bool initImGui();
    bool initSubsystems();
    void processEvents();
    void update();
    void render();
};
```

---

## Avatar Renderer (`src/avatar/`)

The avatar renderer is fully implemented. It renders a rigged GLTF 2.0 humanoid
to an OpenGL FBO and displays it inside the ImGui avatar panel.

### Class interface

```cpp
// src/avatar/AvatarRenderer.h
namespace avatar {
class AvatarRenderer {
public:
    // Load GLB model + compile shaders. Call after GL context is current.
    bool init(const std::string& glbPath);

    // Render avatar to FBO. Call BEFORE ImGui::NewFrame() each frame.
    void render(float width, float height, float dt);

    // Returns FBO colour texture for ImGui::Image().
    ImTextureID getTexture() const;

    // Override pose from MediaPipe landmarks (one frame, bypasses animation).
    // boneMatrices: final skin matrices (globalTransform * inverseBindMatrix).
    void setPose(std::span<const glm::mat4> boneMatrices);

    // Release GL resources. Call before ImGui_ImplOpenGL3_Shutdown().
    void shutdown();

    int  jointCount() const;
    bool isReady()    const;
};
}
```

### GL isolation (critical)

`AvatarRenderer.h` is safe to include from `main.cpp` because it uses **pImpl** —
all `GLuint` members and `<GL/glew.h>` are confined to `AvatarRenderer.cpp`.
This prevents conflicts with `imgui_impl_opengl3_loader.h`, which defines the same
GL symbols and is pulled in transitively by `<imgui_impl_opengl3.h>`.

**Rule:** Never include `<GL/glew.h>` in the same translation unit as
`<imgui_impl_opengl3.h>`.

### UV flip

OpenGL FBOs have y=0 at the bottom; ImGui expects y=0 at the top.
Always call `ImGui::Image` with flipped UVs:

```cpp
ImGui::Image(avatarRenderer.getTexture(),
             ImGui::GetContentRegionAvail(),
             ImVec2(0, 1),   // uv0 — top-left of display = bottom-left of texture
             ImVec2(1, 0));  // uv1 — bottom-right of display = top-right of texture
```

### Wiring MediaPipe (Phase 3)

```cpp
// When MediaPipe landmarks are available, convert to skin matrices and pass in:
std::vector<glm::mat4> skinMats = landmarkExtractor.toSkinMatrices(landmarks);
avatarRenderer.setPose(skinMats);
// Then call render() — pose override takes effect for this frame.
avatarRenderer.render(w, h, dt);
```

---

## Pipeline Manager

The pipeline manager orchestrates which translation directions are active and routes data between them.

```cpp
// src/pipeline/PipelineManager.h
#pragma once
#include "asl/LandmarkExtractor.h"
#include "asl/GestureClassifier.h"
#include "asl/SentenceAssembler.h"
#include "stt/WhisperEngine.h"
#include "tts/TTSEngine.h"
#include "sta/TextToGloss.h"
#include "FrameData.h"
#include <functional>

enum class TranslationMode {
    ASL_TO_TEXT,       // Signs → English text
    ASL_TO_SPEECH,     // Signs → Spoken English
    SPEECH_TO_TEXT,    // Audio → English text
    SPEECH_TO_ASL,     // Audio → ASL gloss display
    TEXT_TO_SPEECH,    // Typed text → Audio
    TEXT_TO_ASL,       // Typed text → ASL gloss display
    FULL_DUPLEX,       // ASL↔Speech bidirectional
    CAPTION_MODE,      // All → Text (everyone gets captions)
    VOICE_MODE         // ASL→Speech + Speech→Text
};

struct PipelineResult {
    std::string english_text;
    std::string asl_gloss;
    std::vector<float> audio_output;
    cv::Mat annotated_frame;
    float confidence;
    double latency_ms;
};

class PipelineManager {
public:
    PipelineManager(ModelManager& models, Config& config);

    void setMode(TranslationMode mode);
    TranslationMode getMode() const;

    PipelineResult tick(const cv::Mat& camera_frame,
                        const std::vector<float>& audio_buffer,
                        const std::string& text_input);

    using TextCallback  = std::function<void(const std::string&)>;
    using AudioCallback = std::function<void(const std::vector<float>&)>;

    void onTextOutput(TextCallback cb);
    void onAudioOutput(AudioCallback cb);

private:
    TranslationMode mode_ = TranslationMode::ASL_TO_TEXT;

    std::unique_ptr<LandmarkExtractor> landmarks_;
    std::unique_ptr<GestureClassifier> classifier_;
    std::unique_ptr<SentenceAssembler> assembler_;
    std::unique_ptr<WhisperEngine>     whisper_;
    std::unique_ptr<TTSEngine>         tts_;
    std::unique_ptr<TextToGloss>       gloss_;

    TextCallback  text_callback_;
    AudioCallback audio_callback_;

    PipelineResult runASLToText(const cv::Mat& frame);
    PipelineResult runASLToSpeech(const cv::Mat& frame);
    PipelineResult runSTT(const std::vector<float>& audio);
    PipelineResult runSpeechToASL(const std::vector<float>& audio);
    PipelineResult runTTS(const std::string& text);
    PipelineResult runTextToASL(const std::string& text);
};
```

---

## Threading Model

Real-time video + audio processing requires careful threading. The application uses a producer-consumer model with lock-free ring buffers.

```
┌────────────────────────┐
│  Main Thread           │  SDL events + ImGui rendering + avatar FBO render
│  (UI + orchestration)  │  ~60 fps
└──────────┬─────────────┘
           │ reads results from
           │
┌──────────▼─────────────┐
│  Pipeline Thread       │  Runs the active translation pipeline
│  (processing)          │  Consumes frames/audio, produces results
└──────────┬─────────────┘
           │ reads from
           │
┌──────────▼────────────────────────────────────┐
│  Capture Threads                              │
│  ┌─────────────────┐  ┌────────────────────┐  │
│  │ Camera Thread   │  │ Audio Thread       │  │
│  │ OpenCV capture  │  │ SDL audio callback │  │
│  │ → Ring Buffer   │  │ → Ring Buffer      │  │
│  └─────────────────┘  └────────────────────┘  │
└───────────────────────────────────────────────┘
           │
┌──────────▼─────────────┐
│  Output Thread         │  Writes to virtual camera/microphone
│  (non-blocking)        │  Separate to avoid blocking pipeline
└────────────────────────┘
```

### Ring Buffer for Frame Passing

```cpp
// src/utils/RingBuffer.h
#pragma once
#include <array>
#include <atomic>
#include <optional>

template<typename T, size_t Size>
class RingBuffer {
public:
    bool push(const T& item) {
        auto current_write = write_pos_.load(std::memory_order_relaxed);
        auto next = (current_write + 1) % Size;
        if (next == read_pos_.load(std::memory_order_acquire)) {
            return false;  // Buffer full — drop frame
        }
        buffer_[current_write] = item;
        write_pos_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        auto current_read = read_pos_.load(std::memory_order_relaxed);
        if (current_read == write_pos_.load(std::memory_order_acquire)) {
            return std::nullopt;  // Buffer empty
        }
        T item = buffer_[current_read];
        read_pos_.store((current_read + 1) % Size, std::memory_order_release);
        return item;
    }

private:
    std::array<T, Size> buffer_;
    std::atomic<size_t> write_pos_{0};
    std::atomic<size_t> read_pos_{0};
};
```

---

## Dear ImGui UI Layout

The UI is fully responsive, switching layout at `COMPACT_WIDTH = 640 px` every frame
based on `io.DisplaySize.x`. All panels are locked (`NoMove | NoResize`) and recalculate
their position and size every frame (`ImGuiCond_Always`), so they stretch with the window.

### Desktop mode (width ≥ 640 px)

```
┌─ Toolbar ───────────────────────────────────────────────────┐  40 px, full width
│  [Start Capture]  [Stop]   ● Running   60 fps               │
├─ Camera Feed ──────┬─ ASL Avatar ──────┬─ Controls ─────────┤  fills remaining height
│  35% width         │  40% width        │  25% width         │  minus captions strip
│                    │                   │                    │
│  [dark rect]       │  Live 3D avatar   │  Language: EN      │
│   No Camera        │  rendered to FBO  │  Mode: ASL -> TX   │
│                    │  (teal placeholder│  Confidence: [bar] │
│                    │   if no .glb)     │                    │
├────────────────────┴───────────────────┤                    │
│  Captions  (75% width, 80 px high)     │  (controls extend) │
│  Recognized text will appear here...   │                    │
└────────────────────────────────────────┴────────────────────┘
```

- Columns are exact fractions: `cam = w×0.35`, `avatar = w×0.40`, `controls = w×0.25`
- Captions sit flush at `y = toolbar_height + top_panels_height`, spanning Camera + Avatar columns
- Font: **Calibri 16 px** (primary) merged with **Segoe UI 16 px** for BMP symbol ranges
- **All labels are plain ASCII or BMP symbols (≤ U+FFFF).** ImGui cannot render
  supplementary-plane emoji (U+1F000+) — they always display as `?`.

### Mobile / compact mode (width < 640 px)

```
┌─ ASL Avatar ──────────────────────┐  fills height - 60 px, full width
│                                   │
│  Live 3D avatar (FBO)             │
│  or teal placeholder              │
│                                   │
├───────────────────────────────────┤  60 px fixed strip
│  Recognized text here...  ● 60fps │
└───────────────────────────────────┘
```

- Two panels only: avatar (top) and captions strip (bottom 60 px)
- Both panels use `ImGuiCond_Always` — they track window size on every resize
- No toolbar in compact mode; status dot + FPS live in the captions strip

### Font setup

```cpp
// Loaded once after ImGui::CreateContext(), before ImGui_ImplOpenGL3_Init()
ImFontConfig cfg;
cfg.OversampleH = 3;
cfg.OversampleV = 2;
cfg.PixelSnapH = true;
static const ImWchar text_ranges[] = {
    0x0020, 0x00FF,  // Basic Latin + Latin Supplement
    0x2000, 0x206F,  // General Punctuation
    0,
};
io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/calibri.ttf", 16.f, &cfg, text_ranges);

// Merge Segoe UI for symbol-only ranges Calibri does not cover
ImFontConfig merge;
merge.MergeMode  = true;
merge.OversampleH = 3;
static const ImWchar symbol_ranges[] = {
    0x2190, 0x23FF,  // Arrows + Misc Technical (⏹)
    0x25A0, 0x26FF,  // Geometric Shapes (▶ ●) + Misc Symbols (⚙)
    0,
};
io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 16.f, &merge, symbol_ranges);
```

> **Static arrays:** Glyph range arrays must be `static` — ImGui holds a raw pointer to
> them across frames. A local (stack) array will cause use-after-free.

> **No fonts after init:** Never add or resize fonts after `ImGui_ImplOpenGL3_Init`.
> The font atlas texture is uploaded to the GPU at that point.

> **`/utf-8` flag:** Without `add_compile_options(/utf-8)` in CMakeLists.txt, MSVC reads
> source as CP1252 and corrupts any character above U+007F in string literals.

### Video Texture Rendering (Camera Feed — planned)

Camera frames are uploaded to an OpenGL texture and displayed in ImGui.
Note `static_cast<ImTextureID>` — `ImTextureID` is `ImU64` in this ImGui build,
not `void*`.

```cpp
// src/ui/widgets/VideoWidget.cpp
void VideoWidget::updateTexture(const cv::Mat& frame) {
    if (texture_id_ == 0) {
        glGenTextures(1, &texture_id_);
    }
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 rgb.cols, rgb.rows, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
}

void VideoWidget::render(float width, float height) {
    // ImTextureID is ImU64 — use static_cast, not reinterpret_cast or (void*)
    ImGui::Image(static_cast<ImTextureID>(texture_id_), ImVec2(width, height));
}
```

### Mode Selector Panel

```cpp
// src/ui/panels/ControlPanel.cpp
void ControlPanel::render(PipelineManager& pipeline) {
    ImGui::Begin("Controls");

    const char* modes[] = {
        "ASL -> Text", "ASL -> Speech", "Speech -> Text",
        "Speech -> ASL", "Text -> Speech", "Text -> ASL",
        "Full Duplex", "Caption Mode", "Voice Mode"
    };

    int current = static_cast<int>(pipeline.getMode());
    if (ImGui::Combo("Translation Mode", &current, modes, IM_ARRAYSIZE(modes))) {
        pipeline.setMode(static_cast<TranslationMode>(current));
    }

    ImGui::Separator();
    static float confidence = 0.7f;
    ImGui::SliderFloat("Min Confidence", &confidence, 0.3f, 0.95f);

    static bool vcam_enabled = false;
    ImGui::Checkbox("Virtual Camera", &vcam_enabled);

    static bool vmic_enabled = false;
    ImGui::Checkbox("Virtual Microphone", &vmic_enabled);

    ImGui::End();
}
```

---

## Configuration System

Runtime configuration stored as JSON, loaded on startup, editable via UI:

```json
// resources/default_config.json
{
  "camera": {
    "device_index": 0,
    "resolution": [640, 480],
    "fps": 30
  },
  "pipeline": {
    "default_mode": "ASL_TO_TEXT",
    "confidence_threshold": 0.7,
    "sequence_length": 60,
    "dedup_window_ms": 500
  },
  "audio": {
    "sample_rate": 16000,
    "channels": 1,
    "buffer_size": 1024
  },
  "tts": {
    "engine": "piper",
    "voice": "en_US-lessac-medium",
    "speed": 1.0
  },
  "stt": {
    "model": "whisper-base",
    "language": "en"
  },
  "output": {
    "virtual_camera": false,
    "virtual_microphone": false,
    "caption_font_size": 24,
    "show_landmarks": true
  },
  "avatar": {
    "model_path": "resources/models/avatar/avatar.glb",
    "animation_index": 0
  },
  "logging": {
    "level": "info",
    "file": "visear.log"
  }
}
```

---

## Error Handling Strategy

The application uses a consistent error-handling approach across all subsystems:

```cpp
// src/utils/Result.h
#pragma once
#include <variant>
#include <string>

template<typename T>
class Result {
public:
    static Result Ok(T value)         { return Result(std::move(value)); }
    static Result Err(std::string err) { return Result(std::move(err)); }

    bool isOk()  const { return std::holds_alternative<T>(data_); }
    bool isErr() const { return std::holds_alternative<std::string>(data_); }

    const T&           value() const { return std::get<T>(data_); }
    const std::string& error() const { return std::get<std::string>(data_); }

private:
    explicit Result(T value)       : data_(std::move(value)) {}
    explicit Result(std::string e) : data_(std::move(e)) {}
    std::variant<T, std::string> data_;
};
```

Usage throughout the codebase:

```cpp
Result<Landmarks> result = extractor.extract(frame);
if (result.isErr()) {
    spdlog::warn("Landmark extraction failed: {}", result.error());
    return PipelineResult{};  // Skip this frame gracefully
}
auto landmarks = result.value();
```

The application should never crash due to a single bad frame or audio glitch.
Every pipeline stage returns a `Result` and failures are logged and skipped gracefully.
The avatar renderer follows the same pattern — `init()` returns `bool` and the
app falls back to the placeholder on failure.
