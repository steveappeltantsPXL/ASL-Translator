# Mobile Roadmap

## Strategy

Mobile is a **Phase 2** target. The desktop application must be stable and the ML models proven before investing in mobile. The good news: the architecture is designed so that the core ML pipeline ports cleanly to mobile.

### Why Desktop First

- Real-time sign language translation needs sustained camera angle (webcam on desk/laptop)
- GPU resources are more abundant on desktop
- Easier to develop, debug, and iterate on ML pipelines
- Virtual camera/microphone integration requires desktop OS
- The primary use case (video calls, streaming) is desktop-first

### Why Mobile Eventually

- In-person conversations (restaurant, doctor's office, store)
- Portable accessibility tool
- Tablet as a mounted translation station
- Broader user reach (more mobile users than desktop)

---

## What Carries Over From Desktop

The core architecture is designed for maximum code reuse:

| Component                 | Desktop (C++)    | Android                | iOS                      | Reuse                      |
| ------------------------- | ---------------- | ---------------------- | ------------------------ | -------------------------- |
| ONNX models (.onnx files) | ONNX Runtime C++ | ONNX Runtime Mobile    | ONNX Runtime + CoreML    | 100% — same model files    |
| MediaPipe                 | C++ API          | Android SDK (official) | iOS SDK (official)       | Logic reuse, different API |
| Feature processing        | Custom C++       | NDK (same C++)         | Shared C++ via framework | ~90%                       |
| whisper.cpp               | C++              | NDK (same C++)         | C++ via Swift bridge     | ~95%                       |
| Piper TTS                 | C++              | NDK or Android TTS     | iOS AVSpeechSynthesizer  | Partial                    |
| UI                        | Dear ImGui + SDL | Jetpack Compose        | SwiftUI                  | 0% — platform native       |
| Virtual devices           | Platform drivers | N/A (not applicable)   | N/A                      | 0%                         |

### Shared C++ Core

The key insight is to extract a **platform-independent C++ library** from the desktop app that contains all the ML and processing logic:

```
libvisear-core (C++ static library)
├── LandmarkExtractor (MediaPipe wrapper)
├── FeatureProcessor
├── GestureClassifier (ONNX Runtime wrapper)
├── SentenceAssembler
├── WhisperSTT (whisper.cpp wrapper)
├── TTSEngine (Piper wrapper)
└── TextToGloss

This library links into:
├── Desktop app (Dear ImGui + SDL)
├── Android app (via JNI / NDK)
└── iOS app (via C++ framework / Swift bridge)
```

---

## Android Implementation

### Architecture

```
┌─────────────────────────────────────────┐
│  Android App (Kotlin + Jetpack Compose) │
│                                         │
│  UI Layer:                              │
│  - CameraX for camera preview           │
│  - Jetpack Compose for UI               │
│  - Material Design 3                    │
│                                         │
│  Bridge Layer (JNI):                    │
│  - Kotlin ←→ C++ function calls         │
│  - Frame data passed via direct buffers │
│                                         │
│  Native Layer (C++ via NDK):            │
│  - libvisear-core                       │
│  - ONNX Runtime Mobile                  │
│  - MediaPipe Android C++ or Java SDK    │
│  - whisper.cpp                          │
│                                         │
└─────────────────────────────────────────┘
```

### Key Differences From Desktop

**Camera:** Android uses CameraX API (Kotlin) instead of OpenCV. Frames are passed to the native layer via JNI as byte buffers.

**MediaPipe:** Google provides an official Android SDK for MediaPipe, which is better optimized for mobile than the C++ API running on Android. Consider using the Java/Kotlin MediaPipe API directly instead of through the C++ core.

**ONNX Runtime Mobile:** A stripped-down version of ONNX Runtime optimized for mobile. Supports NNAPI (Android Neural Networks API) for hardware acceleration on supported devices.

**GPU:** Use NNAPI execution provider or Qualcomm QNN for Snapdragon devices. No CUDA on mobile.

**No Virtual Devices:** Android doesn't support virtual cameras or microphones in the same way. Instead, the app operates as a standalone tool — the user points their phone camera at a signer and sees text on screen. For calls, the app would need platform-specific integrations (which are limited on mobile).

### Build Setup

```
android/
├── app/
│   ├── src/main/
│   │   ├── java/com/visear/translator/
│   │   │   ├── MainActivity.kt
│   │   │   ├── ui/
│   │   │   │   ├── CameraScreen.kt
│   │   │   │   ├── CaptionOverlay.kt
│   │   │   │   └── SettingsScreen.kt
│   │   │   └── bridge/
│   │   │       └── NativeBridge.kt        # JNI interface
│   │   ├── cpp/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── jni_bridge.cpp             # JNI function implementations
│   │   │   └── (links to libvisear-core)
│   │   └── assets/
│   │       └── models/                    # ONNX model files
│   └── build.gradle.kts
├── build.gradle.kts
└── settings.gradle.kts
```

### JNI Bridge Example

```kotlin
// android/app/src/main/java/com/visear/translator/bridge/NativeBridge.kt
class NativeBridge {
    companion object {
        init {
            System.loadLibrary("visear-core")
        }
    }

    external fun initPipeline(modelPath: String): Boolean
    external fun processFrame(frameData: ByteArray, width: Int, height: Int): TranslationResult
    external fun processAudio(samples: FloatArray, sampleRate: Int): String
    external fun shutdown()
}

data class TranslationResult(
    val text: String,
    val confidence: Float,
    val latencyMs: Double
)
```

---

## iOS Implementation

### Architecture

```
┌─────────────────────────────────────────┐
│  iOS App (Swift + SwiftUI)              │
│                                         │
│  UI Layer:                              │
│  - AVCaptureSession for camera          │
│  - SwiftUI for UI                       │
│  - Native iOS design patterns           │
│                                         │
│  Bridge Layer:                          │
│  - Swift ←→ C++ via Objective-C++ or    │
│    Swift/C++ interop (Swift 5.9+)       │
│                                         │
│  Native Layer (C++ framework):          │
│  - libvisear-core                       │
│  - ONNX Runtime (CoreML provider)       │
│  - Apple Vision framework (alternative  │
│    to MediaPipe for pose estimation)    │
│  - whisper.cpp (Metal acceleration)     │
│                                         │
│  Apple-Specific Accelerations:          │
│  - CoreML for model inference           │
│  - Metal for GPU compute                │
│  - Neural Engine for ML                 │
│  - AVSpeechSynthesizer for TTS          │
│                                         │
└─────────────────────────────────────────┘
```

### Key Differences From Desktop

**Pose Estimation:** Apple's Vision framework provides VNDetectHumanHandPoseRequest and VNDetectHumanBodyPoseRequest that run on the Neural Engine. This could replace MediaPipe entirely on iOS, with better performance and no additional dependencies.

**Model Inference:** ONNX Runtime on iOS uses the CoreML execution provider, which automatically leverages the Neural Engine. Alternatively, convert ONNX to CoreML format using `coremltools` for native CoreML inference.

**TTS:** iOS has AVSpeechSynthesizer built-in with high-quality voices. No need to bundle Piper TTS on iOS unless specific voice customization is required.

**Metal:** whisper.cpp supports Metal acceleration on Apple Silicon, providing excellent STT performance.

### Build Setup

```
ios/
├── VisearTranslator/
│   ├── App/
│   │   ├── VisearTranslatorApp.swift
│   │   └── ContentView.swift
│   ├── Views/
│   │   ├── CameraView.swift
│   │   ├── CaptionView.swift
│   │   └── SettingsView.swift
│   ├── Bridge/
│   │   ├── VisearBridge.swift
│   │   └── VisearBridge.mm           # Objective-C++ bridge
│   ├── Models/
│   │   └── (CoreML models or ONNX)
│   └── Resources/
│       └── models/
├── VisearCore/                        # C++ framework target
│   ├── include/
│   │   └── visear_core.h
│   └── src/
│       └── (links to libvisear-core)
└── VisearTranslator.xcodeproj
```

---

## Mobile-Specific Use Cases

### In-Person Translation

The primary mobile use case is different from desktop. Instead of video calls, it's face-to-face:

```
┌──────────────┐
│   Phone      │
│   Camera     │  ← Points at the signer
│     │        │
│     ▼        │
│  Recognize   │
│  signs       │
│     │        │
│     ▼        │
│  Display     │  ← Text on screen (and/or speak aloud)
│  "Hello"     │
└──────────────┘
```

The user holds or mounts the phone so the camera captures the signer. Translated text appears on screen in real-time, and optionally TTS speaks it aloud through the phone speaker.

### Tablet as Translation Station

A tablet mounted on a stand becomes a dedicated translation station — ideal for service desks, hospital reception, or retail:

```
┌────────────────────────┐
│ ┌────────────────────┐ │
│ │                    │ │
│ │    Camera view     │ │  ← Customer signs here
│ │                    │ │
│ ├────────────────────┤ │
│ │ "I need help       │ │  ← Staff reads translation
│ │  finding room 302" │ │
│ ├────────────────────┤ │
│ │ [Speak] [Type]     │ │  ← Staff can respond
│ └────────────────────┘ │
│       iPad / Tablet    │
└────────────────────────┘
```

---

## Performance Considerations on Mobile

### Latency Budget (Mobile)

Mobile has a tighter thermal and power budget. Target latencies are relaxed compared to desktop:

| Stage             | Desktop Target | Mobile Target | Notes                               |
| ----------------- | -------------- | ------------- | ----------------------------------- |
| Camera capture    | < 5 ms         | < 10 ms       | CameraX/AVCapture overhead          |
| Pose estimation   | < 20 ms        | < 30 ms       | MediaPipe mobile / Vision framework |
| ONNX inference    | < 15 ms        | < 30 ms       | NNAPI / CoreML / Neural Engine      |
| Sentence assembly | < 1 ms         | < 2 ms        | Negligible                          |
| Total pipeline    | < 100 ms       | < 150 ms      | Still feels real-time               |

### Model Optimization for Mobile

The desktop models may be too large or slow for mobile. Strategies:

- **Quantization:** Convert FP32 models to INT8 using ONNX Runtime quantization tools. 2-4x speed improvement with minimal accuracy loss.
- **Pruning:** Remove low-importance weights. Reduces model size by 30-50%.
- **Knowledge distillation:** Train a smaller "student" model that learns from the larger "teacher" model.
- **Separate mobile models:** Maintain a `gesture_classifier_mobile.onnx` alongside `gesture_classifier.onnx` with fewer layers/parameters.

```python
# Example: Quantize for mobile
from onnxruntime.quantization import quantize_dynamic, QuantType

quantize_dynamic(
    "gesture_classifier.onnx",
    "gesture_classifier_mobile.onnx",
    weight_type=QuantType.QInt8
)
```

### Battery Considerations

- Reduce camera FPS to 15 when on battery (every other frame)
- Lower resolution (320x240 instead of 640x480)
- Disable face landmarks if not needed for the current sign vocabulary
- Use platform-native inference (NNAPI/CoreML) instead of CPU fallback
- Implement idle detection — stop processing when no hands are visible

---

## Development Timeline

### Suggested Phases

```
Phase 1: Desktop Application (Months 1-6)
├── Core pipeline: camera → MediaPipe → ONNX → text
├── Dear ImGui UI with mode selection
├── Virtual camera output
├── whisper.cpp STT integration
├── Piper TTS integration
└── Basic settings and configuration

Phase 2: Backend & Analytics (Months 4-8, overlaps with Phase 1)
├── FastAPI server deployment
├── Model versioning and OTA updates
├── Analytics telemetry
├── Feedback collection
└── Admin dashboard

Phase 3: Extract C++ Core Library (Months 7-9)
├── Refactor desktop app into libvisear-core + UI
├── Define clean C API boundary for mobile bridges
├── Comprehensive testing of core library
└── Documentation for mobile integration

Phase 4: Android App (Months 9-12)
├── CameraX + Jetpack Compose UI
├── JNI bridge to libvisear-core
├── NNAPI optimization
├── Mobile model quantization
└── Play Store release

Phase 5: iOS App (Months 12-15)
├── SwiftUI + AVCapture UI
├── CoreML / Vision framework integration
├── Objective-C++ or Swift/C++ bridge
├── Neural Engine optimization
└── App Store release
```

Note: these timelines assume a small team (1-3 developers). Adjust based on team size and availability.

---

## Shared Resources Between Platforms

| Resource                   | Shared?      | Notes                                               |
| -------------------------- | ------------ | --------------------------------------------------- |
| ONNX model files           | Yes          | Same models, possibly quantized versions for mobile |
| ASL dictionary (SQLite)    | Yes          | Same database file                                  |
| Training pipeline (Python) | Yes          | One training pipeline serves all platforms          |
| API server                 | Yes          | Same backend for desktop and mobile clients         |
| Feature extraction logic   | Yes          | Via libvisear-core C++ library                      |
| UI code                    | No           | Platform-native on each (ImGui, Compose, SwiftUI)   |
| Camera capture             | No           | Platform-specific APIs                              |
| Virtual devices            | Desktop only | Not applicable on mobile                            |
