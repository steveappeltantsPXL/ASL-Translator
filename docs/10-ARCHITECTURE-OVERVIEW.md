# Visear ASL Translator — Architecture Overview

## Project Vision

A real-time, bidirectional sign language translation system built as a native C++ desktop application. The system translates between American Sign Language (ASL) and spoken/written English across four directional pipelines, enabling seamless communication between deaf/hard-of-hearing and hearing individuals — including integration with video conferencing platforms.

---

## Translation Directions

The system supports four core translation pipelines. Users select the active pipeline(s) based on their situation.

```
┌──────────────────────────────────────────────────────────────┐
│                  TRANSLATION DIRECTIONS                      │
│                                                              │
│  ASL → Text  (ATS)    Signs captured → English text output   │
│  ASL → Speech (ATS+)  Signs captured → Spoken English audio  │
│  Speech → Text (STT)  Spoken audio → English text display    │
│  Speech → ASL (STA)   Spoken audio → ASL gloss/avatar        │
│  Text → ASL   (TTA)   Typed text → ASL gloss/avatar          │
│  Text → Speech (TTS)  Typed text → Spoken audio output       │
│                                                              │
│  Combined modes:                                             │
│  Full Duplex:  ASL→Text/Speech + Speech→Text/ASL             │
│  Caption Mode: ASL→Text + STT (all parties get text)         │
│  Voice Mode:   ASL→Speech + Speech→Text (natural call feel)  │
└──────────────────────────────────────────────────────────────┘
```

### When to Use Each Mode

| Scenario                                         | Recommended Mode      | Pipelines Active                   |
| ------------------------------------------------ | --------------------- | ---------------------------------- |
| Video call (deaf user with hearing participants) | Voice Mode            | ASL→Speech + STT                   |
| Classroom / lecture (deaf student)               | Caption Mode          | STT only (or + ASL→Text)           |
| In-person conversation                           | Full Duplex           | ASL→Text/Speech + Speech→Text      |
| Streaming / Twitch                               | Caption Mode          | ASL→Text overlay on virtual camera |
| Medical consultation                             | Full Duplex + logging | All pipelines + transcript save    |
| Presentation (deaf presenter)                    | Voice Mode            | ASL→Speech via virtual microphone  |

---

## High-Level Architecture

```
┌───────────────────────────────────────────────────────────────────┐
│                    DESKTOP APPLICATION (C++)                      │
│                    Dear ImGui + SDL3 + OpenGL                     │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │                    INPUT LAYER                              │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐   │  │
│  │  │ Camera Feed  │  │ Microphone   │  │ Text Input       │   │  │
│  │  │ (OpenCV)     │  │ (SDL Audio / │  │ (ImGui Text Box) │   │  │
│  │  │              │  │  PortAudio)  │  │                  │   │  │
│  │  └──────┬───────┘  └──────┬───────┘  └────────┬─────────┘   │  │
│  └─────────┼─────────────────┼───────────────────┼─────────────┘  │
│            │                 │                   │                │
│            │                 │                   │                │
│  ┌─────────▼─────────────────▼───────────────────▼─────────────┐  │
│  │                  PROCESSING LAYER                           │  │
│  │                                                             │  │
│  │  ASL Pipeline:          Audio Pipeline:     Text Pipeline:  │  │
│  │  MediaPipe C++ →        whisper.cpp →       Input text →    │  │
│  │  Landmark Extract →     Transcription →     NLP Parse →     │  │
│  │  ONNX Classifier →      NLP Cleanup        ASL Gloss Map    │  │
│  │  Sentence Assembly                                          │  │
│  │                                                             │  │
│  └─────────┬─────────────────┬───────────────────┬─────────────┘  │
│            │                 │                   │                │
│  ┌─────────▼─────────────────▼───────────────────▼─────────────┐  │
│  │                   OUTPUT LAYER                              │  │
│  │                                                             │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐   │  │
│  │  │ Text Display │  │ TTS Engine   │  │ ASL Visual Out   │   │  │
│  │  │ Captions on  │  │ (Piper/      │  │ Gloss text or    │   │  │
│  │  │ video + UI   │  │  Sherpa-onnx)│  │ Avatar animation │   │  │
│  │  └──────┬───────┘  └──────┬───────┘  └────────┬─────────┘   │  │
│  └─────────┼─────────────────┼───────────────────┼─────────────┘  │
│            │                 │                   │                │
│  ┌─────────▼─────────────────▼───────────────────▼─────────────┐  │
│  │                 INTEGRATION LAYER                           │  │
│  │                                                             │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐   │  │
│  │  │ Virtual Cam  │  │ Virtual Mic  │  │ Platform APIs    │   │  │
│  │  │ (OBS VCam /  │  │ (VB-Audio /  │  │ (Zoom Captions / │   │  │
│  │  │  v4l2loop)   │  │  BlackHole)  │  │  Twitch Chat)    │   │  │
│  │  └──────────────┘  └──────────────┘  └──────────────────┘   │  │
│  └─────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │                    ImGui CONTROL PANEL                      │  │
│  │  Mode selector | Settings | Preview | Dictionary | Analytics│  │
│  └─────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────┘
         │                    │                    │
         ▼                    ▼                    ▼
   Teams / Zoom         Twitch / OBS         Discord / Meet
   (sees virtual        (stream with          (caption bot /
    camera + mic)        captions)             voice output)
```

---

## Technology Stack Summary

| Layer                  | Technology                            | Purpose                               |
| ---------------------- | ------------------------------------- | ------------------------------------- |
| UI Framework           | Dear ImGui + SDL3 + OpenGL            | Real-time GUI, windowing, input       |
| Camera                 | OpenCV 4.x (C++)                      | Frame capture, preprocessing          |
| Pose Estimation        | MediaPipe C++                         | Hand/body/face landmark extraction    |
| Gesture Classification | ONNX Runtime C++                      | Run trained sign language models      |
| Speech-to-Text         | whisper.cpp                           | Local, offline speech recognition     |
| Text-to-Speech         | Piper TTS / Sherpa-onnx               | Local, offline speech synthesis       |
| NLP Post-processing    | Custom C++ + ICU                      | Grammar correction, sentence assembly |
| Virtual Camera         | OBS VCam / v4l2loopback / CoreMediaIO | Video output to other apps            |
| Virtual Microphone     | VB-Audio / BlackHole / PulseAudio     | Audio output to other apps            |
| Build System           | CMake                                 | Cross-platform builds                 |
| Package Manager        | vcpkg                                 | C++ dependency management             |
| IDE                    | CLion                                 | Primary development environment       |
| ML Training            | Python + PyTorch (separate)           | Model training and ONNX export        |

---

## Platform Support

| Platform              | Status  | Notes                                                  |
| --------------------- | ------- | ------------------------------------------------------ |
| Windows 10/11         | Primary | DirectML GPU acceleration, OBS Virtual Camera          |
| macOS (Apple Silicon) | Primary | CoreML/Metal acceleration, CoreMediaIO virtual cam     |
| Linux (Ubuntu/Fedora) | Primary | CUDA acceleration, v4l2loopback virtual cam            |
| Android               | Future  | After desktop is stable, via NDK + MediaPipe Android   |
| iOS                   | Future  | After desktop is stable, via CoreML + Vision framework |

---

## Document Index

| Document                  | Contents                                                        |
| ------------------------- | --------------------------------------------------------------- |
| `02-PROJECT-STRUCTURE.md` | CLion project layout, CMake configuration, directory structure  |
| `03-ML-PIPELINE.md`       | Machine learning architecture, training workflow, model details |
| `04-APPLICATION-GUIDE.md` | Desktop app architecture, pipeline implementation, ImGui UI     |
| `05-API-SERVER.md`        | Backend API design, analytics, model management                 |
| `06-INTEGRATION-GUIDE.md` | Virtual camera/mic, platform integration, bidirectional flow    |
| `07-MOBILE-ROADMAP.md`    | Mobile strategy, NDK/iOS approach, shared code plan             |
| `08-GETTING-STARTED.md`   | Environment setup, first build, development workflow            |
