# Desktop Application Guide

## Application Lifecycle

```
main()
  │
  ▼
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)
  │
  ▼
Create SDL Window + OpenGL Context
  │
  ▼
Initialize Dear ImGui (SDL3 + OpenGL3 backend)
  │
  ▼
Initialize subsystems:
  ├── CameraCapture (OpenCV)
  ├── AudioCapture (SDL_Audio)
  ├── ONNXRuntime (load models)
  ├── MediaPipe (landmark extractor)
  ├── WhisperEngine (STT)
  ├── TTSEngine (Piper)
  ├── VirtualCamera (platform-specific)
  ├── VirtualMicrophone (platform-specific)
  └── PipelineManager (orchestrate everything)
  │
  ▼
Main Loop:
  ┌──────────────────────────────────────────┐
  │  1. SDL_PollEvent (input handling)       │
  │  2. PipelineManager.tick()               │
  │     └── runs active translation pipeline │
  │  3. ImGui::NewFrame()                    │
  │  4. Render UI panels                     │
  │  5. ImGui::Render()                      │
  │  6. SDL_GL_SwapWindow()                  │
  └──────────────────────────────────────────┘
  │
  ▼ (on quit)
Cleanup: release cameras, models, virtual devices
SDL_Quit()
```

---

## Main Entry Point

```cpp
// src/main.cpp
#include "app/Application.h"
#include "utils/Logger.h"
#include <cstdlib>

int main(int argc, char* argv[]) {
    Logger::init("visear", spdlog::level::info);

    Application app;
    if (!app.initialize(argc, argv)) {
        LOG_ERROR("Failed to initialize application");
        return EXIT_FAILURE;
    }

    app.run();  // Blocks until quit
    app.shutdown();

    return EXIT_SUCCESS;
}
```

---

## Application Core

```cpp
// src/app/Application.h
#pragma once
#include <SDL3/SDL.h>
#include "ui/UIManager.h"
#include "pipeline/PipelineManager.h"
#include "capture/CameraCapture.h"
#include "capture/AudioCapture.h"
#include "ml/ModelManager.h"
#include "output/VirtualCamera.h"
#include "Config.h"
#include <memory>

class Application {
public:
    bool initialize(int argc, char* argv[]);
    void run();
    void shutdown();

private:
    // SDL
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;

    // Subsystems (order matters for initialization)
    std::unique_ptr<Config> config_;
    std::unique_ptr<ModelManager> models_;
    std::unique_ptr<CameraCapture> camera_;
    std::unique_ptr<AudioCapture> audio_;
    std::unique_ptr<PipelineManager> pipeline_;
    std::unique_ptr<VirtualCamera> vcam_;
    std::unique_ptr<UIManager> ui_;

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
    std::string english_text;          // Recognized/transcribed text
    std::string asl_gloss;             // ASL gloss representation
    std::vector<float> audio_output;   // TTS audio samples
    cv::Mat annotated_frame;           // Frame with captions/landmarks
    float confidence;
    double latency_ms;
};

class PipelineManager {
public:
    PipelineManager(ModelManager& models, Config& config);

    void setMode(TranslationMode mode);
    TranslationMode getMode() const;

    // Called every frame from the main loop
    PipelineResult tick(const cv::Mat& camera_frame,
                        const std::vector<float>& audio_buffer,
                        const std::string& text_input);

    // Callbacks for output routing
    using TextCallback = std::function<void(const std::string&)>;
    using AudioCallback = std::function<void(const std::vector<float>&)>;

    void onTextOutput(TextCallback cb);
    void onAudioOutput(AudioCallback cb);

private:
    TranslationMode mode_ = TranslationMode::ASL_TO_TEXT;

    // Pipeline components
    std::unique_ptr<LandmarkExtractor> landmarks_;
    std::unique_ptr<GestureClassifier> classifier_;
    std::unique_ptr<SentenceAssembler> assembler_;
    std::unique_ptr<WhisperEngine> whisper_;
    std::unique_ptr<TTSEngine> tts_;
    std::unique_ptr<TextToGloss> gloss_;

    // Output callbacks
    TextCallback text_callback_;
    AudioCallback audio_callback_;

    // Pipeline execution
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
│  Main Thread           │  SDL events + ImGui rendering
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

The UI is organized as dockable panels using ImGui's docking branch:

```
┌──────────────────────────────────────────────────────────────┐
│  [Menu Bar]  File | Mode | Settings | Help                   │
├──────────────────────────────────────────────────────────────┤
│                                  │                           │
│      Camera Feed Panel           │    Control Panel          │
│      (live video +               │    ┌───────────────────┐  │
│       landmark overlay)          │    │ Mode: [dropdown]  │  │
│                                  │    │ Camera: [select]  │  │
│                                  │    │ Confidence: 0.7   │  │
│                                  │    │ Virtual Cam: [✓]  │  │
│      [640x480 video texture]     │    │ Virtual Mic: [✓]  │  │
│                                  │    │ TTS Voice: [...]  │  │
│                                  │    └───────────────────┘  │
│                                  │                           │
│                                  │    Dictionary Panel       │
│                                  │    ┌───────────────────┐  │
│                                  │    │ Search: [______]  │  │
│                                  │    │ Recent signs:     │  │
│                                  │    │ - HELLO           │  │
│                                  │    │ - THANK-YOU       │  │
│                                  │    └───────────────────┘  │
├──────────────────────────────────┴───────────────────────────┤
│                                                              │
│    Caption / Output Panel                                    │
│    ┌──────────────────────────────────────────────────────┐  │
│    │ [14:32:01] Hello, how are you today?                 │  │
│    │ [14:32:05] I am fine, thank you.                     │  │
│    │ [14:32:08] ▌  (live recognition indicator)           │  │
│    └──────────────────────────────────────────────────────┘  │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│  [Status Bar]  FPS: 60 | Pipeline: 42ms | GPU: CUDA | Mode   │
└──────────────────────────────────────────────────────────────┘
```

### Video Texture Rendering

Camera frames are uploaded to an OpenGL texture and displayed in ImGui:

```cpp
// src/ui/widgets/VideoWidget.cpp
#include "VideoWidget.h"
#include <GL/gl.h>

void VideoWidget::updateTexture(const cv::Mat& frame) {
    if (texture_id_ == 0) {
        glGenTextures(1, &texture_id_);
    }

    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // OpenCV uses BGR, convert to RGB
    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 rgb.cols, rgb.rows, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
}

void VideoWidget::render(float width, float height) {
    ImGui::Image((ImTextureID)(intptr_t)texture_id_,
                 ImVec2(width, height));
}
```

### Mode Selector Panel

```cpp
// src/ui/panels/ControlPanel.cpp
void ControlPanel::render(PipelineManager& pipeline) {
    ImGui::Begin("Control Panel");

    const char* modes[] = {
        "ASL → Text",
        "ASL → Speech",
        "Speech → Text",
        "Speech → ASL",
        "Text → Speech",
        "Text → ASL",
        "Full Duplex",
        "Caption Mode",
        "Voice Mode"
    };

    int current = static_cast<int>(pipeline.getMode());
    if (ImGui::Combo("Translation Mode", &current, modes, IM_ARRAYSIZE(modes))) {
        pipeline.setMode(static_cast<TranslationMode>(current));
    }

    ImGui::Separator();

    // Confidence threshold
    static float confidence = 0.7f;
    ImGui::SliderFloat("Min Confidence", &confidence, 0.3f, 0.95f);

    // Virtual device toggles
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
  "logging": {
    "level": "info",
    "file": "visear.log"
  }
}
```

---

## Error Handling Strategy

The application uses a consistent error handling approach across all subsystems:

```cpp
// src/utils/Result.h
#pragma once
#include <variant>
#include <string>

template<typename T>
class Result {
public:
    static Result Ok(T value) { return Result(std::move(value)); }
    static Result Err(std::string error) { return Result(std::move(error)); }

    bool isOk() const { return std::holds_alternative<T>(data_); }
    bool isErr() const { return std::holds_alternative<std::string>(data_); }

    const T& value() const { return std::get<T>(data_); }
    const std::string& error() const { return std::get<std::string>(data_); }

private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(std::string error) : data_(std::move(error)) {}
    std::variant<T, std::string> data_;
};
```

Usage throughout the codebase:

```cpp
Result<Landmarks> result = extractor.extract(frame);
if (result.isErr()) {
    LOG_WARN("Landmark extraction failed: {}", result.error());
    return PipelineResult{};  // Skip this frame gracefully
}
auto landmarks = result.value();
```

The application should never crash due to a single bad frame or audio glitch. Every pipeline stage returns a Result and failures are logged and skipped gracefully.
