# Integration Guide — Virtual Devices & Platform Integration

## Overview

The desktop application integrates with video conferencing and streaming platforms through two virtual device abstractions:

- **Virtual Camera** — outputs annotated video (with captions) that any app sees as a webcam
- **Virtual Microphone** — outputs TTS audio that any app sees as a microphone

This approach is platform-agnostic: one implementation works with Teams, Zoom, Discord, Twitch, Google Meet, FaceTime, and any future platform.

---

## Virtual Camera

### How It Works

```
Physical Webcam
    │
    ▼
OpenCV captures frame (640x480, 30fps)
    │
    ▼
Pipeline processes frame:
  - MediaPipe extracts landmarks
  - ONNX classifies gesture
  - Sentence assembler produces text
    │
    ▼
CaptionOverlay renders text onto the frame
    │
    ▼
Annotated frame → Virtual Camera Driver
    │
    ▼
Teams / Zoom / OBS / Discord / Meet
(user selects "Visear ASL Camera" as their video source)
```

### Platform-Specific Implementation

#### Windows — DirectShow or Media Foundation

```cpp
// src/output/platform/VCamWindows.h
#pragma once
#include <opencv2/core.hpp>

class VCamWindows {
public:
    bool initialize(int width, int height, int fps);
    bool writeFrame(const cv::Mat& frame);
    void shutdown();

private:
    // Option 1: Use OBS Virtual Camera SDK
    // Option 2: Custom DirectShow source filter
    // Option 3: Use softcam library (MIT, lightweight)
};
```

### Windows Installation Steps

1.  **Install OBS Studio**: This is the most reliable way to get a signed, high-performance virtual camera driver on Windows.
    - Run `winget install obsproject.obs-studio` or download from [obsproject.com](https://obsproject.com/).
2.  **Enable Virtual Camera**: In OBS, click "Start Virtual Camera" (bottom right). This initializes the driver.
3.  **App Integration**: Our application will detect the "OBS Virtual Camera" device and write annotated frames to it.
4.  _(Alternative)_: For a standalone driver restricted to your own app, consider the [softcam](https://github.com/v002/softcam) library.

#### macOS — CoreMediaIO

```cpp
// src/output/platform/VCamMacOS.h
#pragma once
#include <opencv2/core.hpp>

class VCamMacOS {
public:
    bool initialize(int width, int height, int fps);
    bool writeFrame(const cv::Mat& frame);
    void shutdown();

private:
    // CoreMediaIO DAL plugin
    // Requires a separate .plugin bundle installed in
    // /Library/CoreMediaIO/Plug-Ins/DAL/
};
```

macOS virtual cameras require a CoreMediaIO Device Abstraction Layer (DAL) plugin. This is a separate build artifact that must be installed in the system plugins directory. The main app writes frames to shared memory; the plugin reads them and presents as a camera source.

#### Linux — v4l2loopback

```cpp
// src/output/platform/VCamLinux.h
#pragma once
#include <opencv2/core.hpp>

class VCamLinux {
public:
    bool initialize(int width, int height, int fps);
    bool writeFrame(const cv::Mat& frame);
    void shutdown();

private:
    int device_fd_ = -1;  // /dev/videoN file descriptor
};
```

Linux uses the `v4l2loopback` kernel module. Setup:

```bash
# Install v4l2loopback
sudo apt install v4l2loopback-dkms

# Load the module (creates /dev/video10)
sudo modprobe v4l2loopback video_nr=10 card_label="Visear ASL Camera"
```

The application writes frames directly to the loopback device via `ioctl`/`write` calls. This is the simplest implementation of the three platforms.

### Unified Interface

```cpp
// src/output/VirtualCamera.h
#pragma once
#include <opencv2/core.hpp>
#include <memory>

class VirtualCamera {
public:
    static std::unique_ptr<VirtualCamera> create();

    virtual bool initialize(int width, int height, int fps) = 0;
    virtual bool writeFrame(const cv::Mat& frame) = 0;
    virtual bool isAvailable() const = 0;
    virtual std::string deviceName() const = 0;
    virtual void shutdown() = 0;
    virtual ~VirtualCamera() = default;
};

// Factory creates platform-specific implementation
// VirtualCamera::create() returns VCamWindows / VCamMacOS / VCamLinux
```

---

## Virtual Microphone

### How It Works

```
Sign language recognized → "Hello, how are you?"
    │
    ▼
Piper TTS generates audio waveform (16-bit PCM, 22050 Hz)
    │
    ▼
Audio samples → Virtual Microphone Driver
    │
    ▼
Teams / Zoom / Discord
(user selects "Visear ASL Microphone" as their audio input)
```

### Platform-Specific Solutions

| Platform | Solution                                                       | Notes                                                 |
| -------- | -------------------------------------------------------------- | ----------------------------------------------------- |
| Windows  | **VB-Audio Virtual Cable** (free) or Windows Audio Session API | VB-Audio is easiest; WASAPI loopback for custom       |
| macOS    | **BlackHole** (open-source) or custom CoreAudio driver         | BlackHole creates a virtual audio device              |
| Linux    | **PulseAudio null sink**                                       | `pactl load-module module-null-sink sink_name=visear` |

### Audio Output Pipeline

```cpp
// src/output/VirtualMicrophone.h
#pragma once
#include <vector>
#include <string>
#include <memory>

class VirtualMicrophone {
public:
    static std::unique_ptr<VirtualMicrophone> create();

    virtual bool initialize(int sample_rate, int channels) = 0;
    virtual bool writeSamples(const std::vector<float>& samples) = 0;
    virtual bool isAvailable() const = 0;
    virtual std::string deviceName() const = 0;
    virtual void shutdown() = 0;
    virtual ~VirtualMicrophone() = default;
};
```

---

## Caption Overlay Rendering

The caption overlay renders translated text directly onto video frames before they reach the virtual camera.

```cpp
// src/output/CaptionOverlay.h
#pragma once
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/freetype.hpp>
#include <string>
#include <deque>

struct CaptionLine {
    std::string text;
    double timestamp;
    float confidence;
};

class CaptionOverlay {
public:
    CaptionOverlay();

    void addCaption(const std::string& text, float confidence);
    void renderOnFrame(cv::Mat& frame);

    // Settings
    void setFontSize(int size);
    void setPosition(int x, int y);  // Bottom-center default
    void setBackgroundOpacity(float alpha);
    void setMaxLines(int lines);

private:
    std::deque<CaptionLine> lines_;
    cv::Ptr<cv::freetype::FreeType2> freetype_;
    int font_size_ = 24;
    int max_lines_ = 3;
    float bg_opacity_ = 0.6f;

    void renderBackground(cv::Mat& frame, const cv::Rect& region);
    void renderText(cv::Mat& frame, const std::string& text, cv::Point position);
};
```

### Caption Rendering Approach

```
Original frame (640x480)
┌─────────────────────────────────────┐
│                                     │
│         Video content               │
│                                     │
│                                     │
│                                     │
│  ┌─────────────────────────────┐    │
│  │ ▓▓ Hello, how are you? ▓▓   │    │  ← Semi-transparent background
│  │ ▓▓ I am fine, thank you ▓▓  │    │     with white text
│  └─────────────────────────────┘    │
└─────────────────────────────────────┘
     → Sent to virtual camera
```

---

## Landmark Overlay (Debug/Visual Mode)

Optionally draw MediaPipe landmarks on the video feed for debugging or demonstration:

```cpp
// Part of CameraPanel or DebugPanel
void renderLandmarks(cv::Mat& frame, const MediaPipeLandmarks& landmarks) {
    // Draw hand connections
    for (const auto& connection : HAND_CONNECTIONS) {
        auto& p1 = landmarks.leftHand[connection.first];
        auto& p2 = landmarks.leftHand[connection.second];
        cv::line(frame,
                 cv::Point(p1.x * frame.cols, p1.y * frame.rows),
                 cv::Point(p2.x * frame.cols, p2.y * frame.rows),
                 cv::Scalar(0, 255, 0), 2);  // Green for left hand
    }

    // Similar for right hand (blue), pose (white), face (yellow)
}
```

---

## Platform-Specific Integration APIs

Beyond virtual devices, some platforms offer direct API integration for richer functionality.

### Zoom — Closed Captions API

Zoom supports third-party closed captions via a REST API. Your app can send translated text directly to Zoom's caption system:

```cpp
// POST to Zoom's caption endpoint
// The user provides a caption URL from their Zoom meeting settings
void sendZoomCaption(const std::string& caption_url,
                     const std::string& text,
                     int sequence) {
    json body = {
        {"seq", sequence},
        {"lang", "en-US"},
        {"text", text}
    };
    // HTTP POST to caption_url
    // Captions appear in Zoom's native caption UI
}
```

This means participants can use Zoom's built-in caption controls (toggle, resize, reposition) — a much better UX than burned-in video captions.

### Microsoft Teams — CART Captions

Teams supports Communication Access Real-time Translation (CART) captions. Similar concept to Zoom — send text to an endpoint and it appears in Teams' native caption UI.

### Twitch — Chat Bot + Stream Overlay

For Twitch integration, two approaches:

**Chat Bot:** Post translated text to Twitch chat via IRC:

```
WS connect → irc-ws.chat.twitch.tv:443
PASS oauth:<token>
NICK visear_bot
JOIN #channel_name
PRIVMSG #channel_name :🤟 Hello, how are you?
```

**Stream Overlay:** Write captions to a text file. OBS reads the file as a Text Source and overlays it on the stream. This is the simplest integration and requires zero API work.

### Discord — Bot

A Discord bot can join a voice channel and post translations to a paired text channel:

```
Voice channel: user signs → app recognizes → bot posts text to #captions
Text channel:  others type → app displays in ASL panel
```

---

## Bidirectional Flow — Full Integration Example

Here's the complete data flow for a video call between a deaf signer and a hearing speaker:

```
DEAF USER'S MACHINE                        HEARING USER'S MACHINE
(running Visear)                           (standard Teams/Zoom)

┌──────────────────────┐                   ┌──────────────────────┐
│ Webcam               │                   │ Webcam               │
│   ↓                  │                   │   ↓                  │
│ MediaPipe → ONNX     │                   │ (normal video)       │
│   ↓                  │                   │   ↓                  │
│ "Hello, how are you?"│                   │                      │
│   ↓              ↓   │                   │                      │
│ Captions      Piper  │                   │                      │
│ on video      TTS    │                   │                      │
│   ↓              ↓   │                   │                      │
│ Virtual      Virtual │   ── network ──   │                      │
│ Camera       Mic     │ ================> │ Sees video w/        │
│                      │                   │ captions + hears     │
│                      │                   │ synthesized voice    │
│                      │                   │                      │
│                      │                   │ Hearing user speaks  │
│                      │   ── network ──   │   ↓                  │
│ Receives audio       │ <================ │ Microphone audio     │
│   ↓                  │                   │                      │
│ whisper.cpp (STT)    │                   │                      │
│   ↓                  │                   │                      │
│ Text displayed in    │                   │                      │
│ Visear caption panel │                   │                      │
│ + optional ASL gloss │                   │                      │
└──────────────────────┘                   └──────────────────────┘
```

The hearing user needs **zero additional software**. They see a normal video call where the deaf user has subtitles and a synthesized voice — a seamless experience.

---

## Setup Guide for End Users

The application should include a first-run wizard that guides users through:

1. **Camera selection** — choose which webcam to use
2. **Virtual camera setup** — install/configure the virtual camera driver
3. **Virtual microphone setup** — install/configure the virtual audio device
4. **Mode selection** — choose default translation mode
5. **Platform test** — open a test call to verify everything works
6. **TTS voice selection** — choose preferred voice for speech output

Each step should include platform-specific instructions and a "Test" button to verify the setup works before proceeding.
