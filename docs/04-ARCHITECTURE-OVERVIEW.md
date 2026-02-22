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
│                    Dear ImGui + SDL3 + OpenGL 3.3                 │
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
│  │  │ Text Display │  │ TTS Engine   │  │ ASL Avatar       │   │  │
│  │  │ Captions on  │  │ (Piper/      │  │ Rigged 3D mesh   │   │  │
│  │  │ video + UI   │  │  Sherpa-onnx)│  │ driven by pose   │   │  │
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

## Avatar Renderer Architecture

The ASL Avatar panel uses a dedicated OpenGL rendering pipeline, isolated from ImGui's
own GL loader via the pImpl pattern:

```
Each frame (before ImGui::NewFrame):
  AvatarRenderer::render(panelW, panelH, dt)
  │
  ├─ Resize FBO if panel dimensions changed
  ├─ glBindFramebuffer(FBO) → glViewport → glClear
  ├─ AnimationPlayer::tick() — sample keyframes (linear T/S, slerp R)
  ├─ Compose TRS → local transforms → Skeleton::computeSkinMatrices()
  ├─ glUniformMatrix4fv(u_BoneMatrices[100]) — skinned mesh shader
  ├─ Draw each SkinnedMesh (VAO/VBO with glVertexAttribIPointer for bone IDs)
  └─ glBindFramebuffer(0) — restore default

ImGui "ASL Avatar" panel:
  ImGui::Image(avatarRenderer.getTexture(), avail, {0,1}, {1,0})
                                                    ↑ UV flip (GL y-origin)
```

When MediaPipe is wired up, `AvatarRenderer::setPose(span<mat4>)` accepts final
skin matrices directly, bypassing the animation system for one frame.

---

## Technology Stack Summary

| Layer                  | Technology                            | Status    | Purpose                               |
| ---------------------- | ------------------------------------- | --------- | ------------------------------------- |
| UI Framework           | Dear ImGui + SDL3 + OpenGL 3.3 core   | ✅ Built   | Real-time GUI, windowing, input       |
| Avatar Renderer        | OpenGL FBO + GLSL 3.30 + tinygltf     | ✅ Built   | Rigged 3D humanoid, idle animation    |
| Math Library           | glm                                   | ✅ Built   | Vectors, matrices, quaternions        |
| GL Extension Loader    | GLEW                                  | ✅ Built   | FBO, VAO, VBO, glVertexAttribIPointer |
| GLTF Loader            | tinygltf (vendored header-only)       | ✅ Built   | GLB model + skeleton + animations     |
| Camera                 | OpenCV 4.x (C++)                      | ⏳ Pending | Frame capture, preprocessing          |
| Pose Estimation        | MediaPipe C++                         | ⏳ Pending | Hand/body/face landmark extraction    |
| Gesture Classification | ONNX Runtime C++                      | ⏳ Pending | Run trained sign language models      |
| Speech-to-Text         | whisper.cpp                           | ⏳ Pending | Local, offline speech recognition     |
| Text-to-Speech         | Piper TTS / Sherpa-onnx               | ⏳ Pending | Local, offline speech synthesis       |
| NLP Post-processing    | Custom C++ + ICU                      | ⏳ Pending | Grammar correction, sentence assembly |
| Virtual Camera         | OBS VCam / v4l2loopback / CoreMediaIO | ⏳ Pending | Video output to other apps            |
| Virtual Microphone     | VB-Audio / BlackHole / PulseAudio     | ⏳ Pending | Audio output to other apps            |
| Build System           | CMake                                 | ✅ Built   | Cross-platform builds                 |
| Package Manager        | vcpkg                                 | ✅ Built   | C++ dependency management             |
| IDE                    | CLion                                 | ✅ Built   | Primary development environment       |
| ML Training            | Python + PyTorch (separate)           | ⏳ Pending | Model training and ONNX export        |

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
| `00-PROJECT-STATUS.md`    | Current build state, what works, what's pending                 |
| `02-GETTING-STARTED.md`   | Environment setup, first build, development workflow            |
| `03-BUILD-COMMANDS.md`    | CMake configure/build reference with error fixes                |
| `05-PROJECT-STRUCTURE.md` | CLion project layout, CMake configuration, directory structure  |
| `06-ML-PIPELINE.md`       | Machine learning architecture, training workflow, model details |
| `07-APPLICATION-GUIDE.md` | Desktop app architecture, pipeline implementation, ImGui UI     |
| `08-API-SERVER.md`        | Backend API design, analytics, model management                 |
| `09-INTEGRATION-GUIDE.md` | Virtual camera/mic, platform integration, bidirectional flow    |
| `10-MOBILE-ROADMAP.md`    | Mobile strategy, NDK/iOS approach, shared code plan             |
