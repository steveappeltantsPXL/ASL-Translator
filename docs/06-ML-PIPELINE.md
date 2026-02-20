# Machine Learning Pipeline

## Overview

The ML system is split into two distinct environments:

- **Training environment** — Python + PyTorch, runs on developer machines or cloud GPU instances
- **Inference environment** — C++ + ONNX Runtime, embedded in the desktop application

The bridge between them is the **ONNX model format**. Train in Python, export to ONNX, deploy in C++.

```
TRAINING (Python)                          INFERENCE (C++)
──────────────────                         ─────────────────
PyTorch model                              ONNX Runtime
    │                                           ▲
    ▼                                           │
torch.onnx.export() ──→ model.onnx ──→ Ort::Session::Run()
```

---

## Models Required

The application requires multiple ML models working in sequence:

| Model              | Input                                | Output                              | Framework                       | Size (approx) |
| ------------------ | ------------------------------------ | ----------------------------------- | ------------------------------- | ------------- |
| Pose Estimation    | RGB video frame                      | 543 landmarks (hands + pose + face) | MediaPipe C++                   | ~10 MB        |
| Gesture Classifier | Landmark sequences (temporal window) | Sign label + confidence             | Custom (PyTorch → ONNX)         | ~5-20 MB      |
| Fingerspelling     | Hand landmarks (single frame)        | Letter A-Z + space                  | Custom (PyTorch → ONNX)         | ~2 MB         |
| Whisper STT        | Audio waveform (16kHz)               | English text                        | whisper.cpp                     | ~75 MB (base) |
| Piper TTS          | Text string                          | Audio waveform                      | Piper / Sherpa-onnx             | ~30 MB        |
| NLP Correction     | Raw token sequence                   | Grammatical English                 | Small transformer or rule-based | ~5 MB         |

### Model Pipeline Flow

```
CAMERA FRAME (30 fps, 640x480)
    │
    ▼
┌─────────────────────────┐
│  MediaPipe Holistic     │  C++ native — runs on CPU or GPU
│  543 landmarks:         │
│  - 33 pose landmarks    │
│  - 21 per hand (x2=42)  │
│  - 468 face landmarks   │
└──────────┬──────────────┘
           │
           ▼
┌─────────────────────────┐
│  Feature Processor      │  C++ — runs on CPU
│                         │
│  - Normalize landmarks  │  (scale/translate invariance)
│  - Compute velocities   │  (temporal derivatives)
│  - Build sliding window │  (last N frames, typically 30-60)
│  - Flatten to tensor    │  [batch, sequence_length, features]
└──────────┬──────────────┘
           │
           ▼
┌─────────────────────────┐
│  Gesture Classifier     │  ONNX Runtime — GPU preferred
│  (Transformer or LSTM)  │
│                         │
│  Input:  [1, 60, 225]   │  (1 batch, 60 frames, 225 features)
│  Output: [1, num_signs] │  (probability per sign class)
└──────────┬──────────────┘
           │
           ├──→ If fingerspelling detected → Fingerspelling Model
           │
           ▼
┌─────────────────────────┐
│  Sentence Assembler     │  C++ — rule-based + optional small model
│                         │
│  - Deduplicate repeated │  (holding a sign = one word, not many)
│    predictions          │
│  - Apply confidence     │  (threshold filtering)
│    threshold            │
│  - Buffer into phrases  │  (detect sign boundaries)
│  - NLP correction       │  (ASL gloss → English grammar)
└──────────┬─────────────┘
           │
           ▼
       English text: "Hello, how are you today?"
```

---

## Training Pipeline (Python)

### Directory Structure

```
training/
├── requirements.txt
├── config/
│   ├── gesture_model.yaml         # Hyperparameters
│   └── fingerspell_model.yaml
├── src/
│   ├── dataset/
│   │   ├── wlasl_loader.py        # WLASL dataset integration
│   │   ├── msasl_loader.py        # MS-ASL dataset integration
│   │   ├── landmark_extractor.py  # Batch extract landmarks from video
│   │   ├── augmentation.py        # Temporal + spatial augmentation
│   │   └── preprocessing.py       # Normalization, windowing
│   ├── models/
│   │   ├── gesture_transformer.py # Transformer-based classifier
│   │   ├── gesture_lstm.py        # LSTM alternative
│   │   ├── fingerspell_cnn.py     # CNN for static hand poses
│   │   └── nlp_correction.py      # Gloss → English model
│   ├── train.py                   # Main training entry point
│   ├── evaluate.py                # Model evaluation + metrics
│   └── export_onnx.py             # PyTorch → ONNX conversion
├── data/
│   ├── raw/                       # Downloaded datasets (gitignored)
│   ├── processed/                 # Extracted landmarks (gitignored)
│   └── splits/                    # Train/val/test splits
├── experiments/                   # MLflow experiment logs
└── notebooks/
    ├── data_exploration.ipynb
    ├── model_comparison.ipynb
    └── error_analysis.ipynb
```

### Requirements

```
# training/requirements.txt
torch>=2.1.0
torchvision>=0.16.0
mediapipe>=0.10.0
onnx>=1.15.0
onnxruntime>=1.16.0
numpy>=1.24.0
pandas>=2.0.0
scikit-learn>=1.3.0
mlflow>=2.8.0
albumentations>=1.3.0
opencv-python>=4.8.0
pyyaml>=6.0
tqdm>=4.66.0
matplotlib>=3.8.0
seaborn>=0.13.0
jupyter>=1.0.0
```

### Training Workflow

```bash
# Step 1: Download datasets
cd training
python src/dataset/wlasl_loader.py --output data/raw/wlasl
python src/dataset/msasl_loader.py --output data/raw/msasl

# Step 2: Extract landmarks from all videos
python src/dataset/landmark_extractor.py \
    --input data/raw/wlasl/videos \
    --output data/processed/wlasl_landmarks \
    --workers 8

# Step 3: Train gesture classifier
python src/train.py \
    --config config/gesture_model.yaml \
    --data data/processed/wlasl_landmarks \
    --experiment gesture_v1

# Step 4: Evaluate
python src/evaluate.py \
    --model experiments/gesture_v1/best_model.pt \
    --data data/splits/test.json

# Step 5: Export to ONNX
python src/export_onnx.py \
    --model experiments/gesture_v1/best_model.pt \
    --output ../resources/models/gesture_classifier.onnx \
    --input-shape 1 60 225 \
    --opset 17
```

### Model Architecture: Gesture Classifier

The recommended architecture is a **Transformer encoder** over landmark sequences:

```
Input: [batch, seq_len=60, features=225]
    │
    ▼
Linear projection (225 → 256)
    │
    ▼
Positional encoding (learned, seq_len=60)
    │
    ▼
Transformer Encoder (4 layers, 4 heads, dim=256)
    │
    ▼
Global average pooling
    │
    ▼
Classification head (256 → num_signs)
    │
    ▼
Output: [batch, num_signs] (softmax probabilities)
```

Why Transformer over LSTM:

- Better at capturing long-range dependencies in sign sequences
- Parallelizable during training (faster)
- Attention weights are interpretable (which frames mattered for classification)
- ONNX export is cleaner than LSTM stateful export

### Feature Vector (225 dimensions)

Per frame, extract from MediaPipe landmarks:

```
Left hand:  21 landmarks × 3 (x, y, z) = 63
Right hand: 21 landmarks × 3 (x, y, z) = 63
Pose:       11 upper body landmarks × 3  = 33  (shoulders, elbows, wrists, etc.)
Face:       22 key landmarks × 3         = 66  (eyebrows, mouth, nose — non-manual markers)
                                          ─────
                                           225 features per frame
```

### ONNX Export

```python
# export_onnx.py (key section)
import torch
import onnx
from onnxruntime import InferenceSession

model = GestureTransformer.load("best_model.pt")
model.eval()

dummy_input = torch.randn(1, 60, 225)

torch.onnx.export(
    model,
    dummy_input,
    "gesture_classifier.onnx",
    opset_version=17,
    input_names=["landmarks"],
    output_names=["predictions"],
    dynamic_axes={
        "landmarks": {0: "batch_size"},
        "predictions": {0: "batch_size"}
    }
)

# Validate
onnx_model = onnx.load("gesture_classifier.onnx")
onnx.checker.check_model(onnx_model)

# Test inference matches
session = InferenceSession("gesture_classifier.onnx")
onnx_output = session.run(None, {"landmarks": dummy_input.numpy()})
torch_output = model(dummy_input).detach().numpy()
assert np.allclose(onnx_output[0], torch_output, atol=1e-5)
```

---

## Inference in C++ (ONNX Runtime)

### Session Management

```cpp
// ml/ONNXRuntime.h
#pragma once
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <memory>

class ONNXInference {
public:
    explicit ONNXInference(const std::string& model_path, bool use_gpu = true);

    // Run inference on a landmark sequence
    // Input: [1, seq_len, features] flattened to vector
    // Output: [1, num_classes] probabilities
    std::vector<float> predict(const std::vector<float>& input,
                                const std::vector<int64_t>& input_shape);

    // Get predicted class and confidence
    std::pair<int, float> predictTopClass(const std::vector<float>& input,
                                           const std::vector<int64_t>& input_shape);

private:
    Ort::Env env_;
    Ort::Session session_;
    Ort::AllocatorWithDefaultOptions allocator_;

    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;

    void configureGPU(Ort::SessionOptions& options);
};
```

### GPU Execution Providers

ONNX Runtime selects GPU backend per platform:

```cpp
void ONNXInference::configureGPU(Ort::SessionOptions& options) {
#ifdef VISEAR_PLATFORM_WINDOWS
    // DirectML — works on any Windows GPU (NVIDIA, AMD, Intel)
    Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(options, 0));
#elif defined(VISEAR_PLATFORM_MACOS)
    // CoreML — optimized for Apple Silicon
    Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CoreML(options, 0));
#elif defined(VISEAR_PLATFORM_LINUX)
    // CUDA — NVIDIA GPUs
    OrtCUDAProviderOptions cuda_options;
    cuda_options.device_id = 0;
    options.AppendExecutionProvider_CUDA(cuda_options);
#endif
    // Fallback: CPU is always available
}
```

---

## Datasets

### Primary Datasets for ASL

| Dataset  | Signs      | Videos       | Source          | License      |
| -------- | ---------- | ------------ | --------------- | ------------ |
| WLASL    | 2,000      | ~21,000      | Word-Level ASL  | Research use |
| MS-ASL   | 1,000      | ~25,000      | Microsoft       | Research use |
| ASL-LEX  | 2,723      | Lexical data | ASL-LEX project | CC-BY        |
| How2Sign | Continuous | ~80 hours    | Continuous ASL  | Research use |
| ASLLVD   | 3,000+     | ~10,000      | Gallaudet/BU    | Research use |

### Data Pipeline

```
Raw Video → MediaPipe Landmark Extraction → Normalized Sequences → Training
                                                                       │
                                                        ┌──────────────┤
                                                        ▼              ▼
                                                  Train Set      Validation Set
                                                   (80%)            (10%)
                                                                       │
                                                                       ▼
                                                                  Test Set (10%)
```

### Data Augmentation Strategies

For sign language data, augmentation must preserve the linguistic meaning:

**Safe augmentations:**

- Temporal scaling (speed up/slow down by ±20%)
- Spatial jitter (small random offset to all landmarks)
- Mirror (left↔right hand swap — must also swap sign labels where applicable)
- Dropout (randomly zero out individual landmarks, simulates occlusion)
- Gaussian noise on landmark positions

**Unsafe augmentations (avoid):**

- Rotation beyond ±15° (changes spatial meaning in ASL)
- Cropping (may remove hands from frame)
- Color augmentation (irrelevant since we use landmarks, not pixels)

---

## Latency Budget

For real-time feel, the total pipeline must stay under 200ms. Target budget:

| Stage                  | Target       | Measured On                           |
| ---------------------- | ------------ | ------------------------------------- |
| Camera capture         | < 5 ms       | 30fps = 33ms/frame, capture is subset |
| MediaPipe landmarks    | < 20 ms      | CPU; ~8ms on GPU                      |
| Feature processing     | < 2 ms       | CPU, optimized C++                    |
| ONNX gesture inference | < 15 ms      | GPU; ~40ms CPU fallback               |
| Sentence assembly      | < 1 ms       | CPU, rule-based                       |
| TTS (if active)        | < 50 ms      | First audio chunk                     |
| Virtual camera output  | < 5 ms       | Memory copy                           |
| **Total**              | **< 100 ms** | GPU path                              |

### Profiling

Use the built-in `Timer` utility to measure each stage:

```cpp
Timer timer;
timer.start("mediapipe");
auto landmarks = extractor.extract(frame);
timer.stop("mediapipe");

timer.start("inference");
auto prediction = classifier.predict(features);
timer.stop("inference");

// Display in ImGui debug panel
timer.renderImGui();  // Shows avg/min/max per stage
```

---

## Model Versioning and Updates

Models are versioned and managed via the backend API (see `05-API-SERVER.md`):

```json
// resources/models/model_registry.json
{
  "gesture_classifier": {
    "version": "1.0.0",
    "file": "gesture_classifier.onnx",
    "input_shape": [1, 60, 225],
    "output_classes": 200,
    "min_confidence": 0.7
  },
  "fingerspelling": {
    "version": "1.0.0",
    "file": "fingerspelling.onnx",
    "input_shape": [1, 63],
    "output_classes": 27
  }
}
```

The desktop app checks the API for model updates on startup and can hot-swap models without restart by loading a new ONNX session while keeping the old one active until the new one is ready.
