# API Server & Analytics Backend

## Overview

The backend server is a separate service that supports the desktop application with:

- Model management (versioning, distribution, updates)
- User analytics and usage telemetry
- Feedback collection for model improvement
- Optional cloud inference fallback
- License management (if commercial)
- ASL dictionary / lexicon API

The backend is **not required** for the desktop app to function. The app works fully offline. The backend enhances the experience.

---

## Technology Stack

| Component        | Technology               | Rationale                                |
| ---------------- | ------------------------ | ---------------------------------------- |
| Language         | Python 3.12+             | ML ecosystem, rapid development          |
| Framework        | FastAPI                  | Async, fast, auto-generated OpenAPI docs |
| Database         | PostgreSQL 16            | Relational data, JSON support, proven    |
| ORM              | SQLAlchemy 2.0 (async)   | Type-safe, async-compatible              |
| Cache            | Redis                    | Session cache, rate limiting, task queue |
| Object Storage   | MinIO (S3-compatible)    | Model binary storage and distribution    |
| Task Queue       | Celery + Redis           | Background jobs (training, analytics)    |
| Migrations       | Alembic                  | Database schema versioning               |
| Containerization | Docker + Docker Compose  | Local dev and deployment                 |
| CI/CD            | GitHub Actions           | Automated testing and deployment         |
| Monitoring       | Prometheus + Grafana     | Server metrics and dashboards            |
| API Docs         | Auto-generated (FastAPI) | OpenAPI / Swagger UI                     |

### Why Python for the Backend?

Even though the desktop app is C++, the backend benefits from Python because:

- ML model retraining pipelines are Python-native
- FastAPI is one of the fastest Python frameworks (async, uvicorn)
- Shares code with the training pipeline (data processing, evaluation)
- Rapid iteration on API endpoints
- The backend is I/O-bound (database, file serving), not CPU-bound — Python is fine

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     API SERVER                               │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  FastAPI Application                                   │  │
│  │                                                        │  │
│  │  /api/v1/models      → Model management & downloads    │  │
│  │  /api/v1/analytics   → Telemetry ingestion             │  │
│  │  /api/v1/feedback    → User correction submissions     │  │
│  │  /api/v1/dictionary  → ASL lexicon lookup              │  │
│  │  /api/v1/health      → Health check                    │  │
│  │  /api/v1/inference   → Cloud inference fallback        │  │
│  └────────────────────────────────────────────────────────┘  │
│             │                      │                         │
│  ┌──────────▼────────┐  ┌──────────▼───────────┐             │
│  │  PostgreSQL       │  │  MinIO (S3)          │             │
│  │  - Users          │  │  - Model binaries    │             │
│  │  - Analytics      │  │  - Training data     │             │
│  │  - Feedback       │  │  - Exported reports  │             │
│  │  - Dictionary     │  │                      │             │
│  └───────────────────┘  └──────────────────────┘             │
│             │                                                │
│  ┌──────────▼────────┐  ┌─────────────────────┐              │
│  │  Redis            │  │  Celery Workers     │              │
│  │  - Cache          │  │  - Model retraining │              │
│  │  - Rate limiting  │  │  - Analytics agg.   │              │
│  │  - Task broker    │  │  - Report generation│              │
│  └───────────────────┘  └─────────────────────┘              │
└──────────────────────────────────────────────────────────────┘
```

---

## API Endpoints

### Model Management

```
GET    /api/v1/models
       → List available models with versions

GET    /api/v1/models/{model_name}/latest
       → Get latest version info + download URL

GET    /api/v1/models/{model_name}/download/{version}
       → Download model binary (from MinIO)

POST   /api/v1/models/{model_name}/check-update
       Body: { "current_version": "1.0.0" }
       → Returns whether an update is available
```

### Analytics Telemetry

```
POST   /api/v1/analytics/event
       Body: {
         "client_id": "uuid",
         "event_type": "sign_recognized",
         "payload": {
           "sign": "HELLO",
           "confidence": 0.92,
           "pipeline_latency_ms": 45,
           "mode": "ASL_TO_TEXT"
         },
         "timestamp": "2026-02-19T10:30:00Z"
       }

POST   /api/v1/analytics/session
       Body: {
         "client_id": "uuid",
         "session_duration_s": 1800,
         "mode": "VOICE_MODE",
         "signs_recognized": 145,
         "avg_confidence": 0.85,
         "avg_latency_ms": 52
       }

GET    /api/v1/analytics/dashboard
       → Aggregated metrics for admin dashboard
```

### Feedback & Model Improvement

```
POST   /api/v1/feedback/correction
       Body: {
         "client_id": "uuid",
         "sign_predicted": "THANK",
         "sign_actual": "PLEASE",
         "landmarks": [...],
         "timestamp": "2026-02-19T10:30:00Z"
       }

GET    /api/v1/feedback/summary
       → Most common misclassifications (for retraining priority)
```

### Dictionary / Lexicon

```
GET    /api/v1/dictionary/search?q=hello
       → Search ASL dictionary entries

GET    /api/v1/dictionary/sign/{sign_id}
       → Get sign details (description, video URL, usage notes)

GET    /api/v1/dictionary/categories
       → List sign categories (greetings, medical, education, etc.)
```

### Cloud Inference (Fallback)

```
POST   /api/v1/inference/classify
       Body: {
         "landmarks": [[...], [...], ...],
         "sequence_length": 60
       }
       → Returns prediction from server-side ONNX Runtime
       (Used when client lacks GPU or for very large models)
```

---

## Database Schema

```sql
-- Users / Clients
CREATE TABLE clients (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    last_seen TIMESTAMPTZ,
    platform VARCHAR(20),        -- windows, macos, linux
    app_version VARCHAR(20),
    model_versions JSONB         -- {"gesture": "1.2.0", "whisper": "base"}
);

-- Model Registry
CREATE TABLE models (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    version VARCHAR(20) NOT NULL,
    description TEXT,
    file_path VARCHAR(500),      -- MinIO path
    file_size_bytes BIGINT,
    sha256 VARCHAR(64),
    input_shape JSONB,
    output_classes INTEGER,
    published_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    is_active BOOLEAN DEFAULT true,
    UNIQUE(name, version)
);

-- Analytics Events
CREATE TABLE analytics_events (
    id BIGSERIAL PRIMARY KEY,
    client_id UUID REFERENCES clients(id),
    event_type VARCHAR(50) NOT NULL,
    payload JSONB,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_analytics_client ON analytics_events(client_id);
CREATE INDEX idx_analytics_type ON analytics_events(event_type);
CREATE INDEX idx_analytics_time ON analytics_events(created_at);

-- Session Summaries
CREATE TABLE sessions (
    id BIGSERIAL PRIMARY KEY,
    client_id UUID REFERENCES clients(id),
    started_at TIMESTAMPTZ NOT NULL,
    duration_seconds INTEGER,
    mode VARCHAR(30),
    signs_recognized INTEGER,
    avg_confidence REAL,
    avg_latency_ms REAL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Feedback / Corrections
CREATE TABLE feedback (
    id BIGSERIAL PRIMARY KEY,
    client_id UUID REFERENCES clients(id),
    sign_predicted VARCHAR(100),
    sign_actual VARCHAR(100),
    landmarks JSONB,             -- Raw landmark data for retraining
    reviewed BOOLEAN DEFAULT false,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- ASL Dictionary
CREATE TABLE dictionary (
    id SERIAL PRIMARY KEY,
    gloss VARCHAR(100) NOT NULL UNIQUE,
    english TEXT,
    description TEXT,
    category VARCHAR(50),
    video_url VARCHAR(500),
    usage_frequency REAL         -- How common the sign is
);
```

---

## Backend Project Structure

```
server/
├── docker-compose.yml
├── Dockerfile
├── requirements.txt
├── alembic.ini
├── alembic/
│   └── versions/
│
├── app/
│   ├── main.py                    # FastAPI app creation
│   ├── config.py                  # Settings (pydantic-settings)
│   ├── database.py                # Async SQLAlchemy engine
│   │
│   ├── api/
│   │   ├── v1/
│   │   │   ├── models.py          # Model management endpoints
│   │   │   ├── analytics.py       # Telemetry endpoints
│   │   │   ├── feedback.py        # Feedback endpoints
│   │   │   ├── dictionary.py      # Lexicon endpoints
│   │   │   ├── inference.py       # Cloud inference endpoints
│   │   │   └── health.py          # Health check
│   │   └── deps.py                # Shared dependencies
│   │
│   ├── models/                    # SQLAlchemy ORM models
│   │   ├── client.py
│   │   ├── model_registry.py
│   │   ├── analytics.py
│   │   ├── feedback.py
│   │   └── dictionary.py
│   │
│   ├── schemas/                   # Pydantic request/response schemas
│   │   ├── model.py
│   │   ├── analytics.py
│   │   ├── feedback.py
│   │   └── dictionary.py
│   │
│   ├── services/                  # Business logic
│   │   ├── model_service.py       # Model version management
│   │   ├── analytics_service.py   # Telemetry aggregation
│   │   ├── feedback_service.py    # Feedback processing
│   │   ├── inference_service.py   # ONNX Runtime server-side
│   │   └── storage_service.py     # MinIO integration
│   │
│   └── tasks/                     # Celery background tasks
│       ├── retrain.py             # Trigger model retraining
│       ├── aggregate.py           # Daily analytics rollup
│       └── export.py              # Generate reports
│
├── tests/
│   ├── conftest.py
│   ├── test_models_api.py
│   ├── test_analytics_api.py
│   └── test_feedback_api.py
│
└── scripts/
    ├── seed_dictionary.py         # Load ASL dictionary data
    └── create_admin.py            # Create admin user
```

### Docker Compose

```yaml
# docker-compose.yml
version: "3.9"

services:
  api:
    build: .
    ports:
      - "8000:8000"
    environment:
      - DATABASE_URL=postgresql+asyncpg://visear:visear@db:5432/visear
      - REDIS_URL=redis://redis:6379
      - MINIO_ENDPOINT=minio:9000
      - MINIO_ACCESS_KEY=minioadmin
      - MINIO_SECRET_KEY=minioadmin
    depends_on:
      - db
      - redis
      - minio
    volumes:
      - ./app:/app/app

  db:
    image: postgres:16
    environment:
      POSTGRES_DB: visear
      POSTGRES_USER: visear
      POSTGRES_PASSWORD: visear
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data

  redis:
    image: redis:7-alpine
    ports:
      - "6379:6379"

  minio:
    image: minio/minio
    command: server /data --console-address ":9001"
    ports:
      - "9000:9000"
      - "9001:9001"
    environment:
      MINIO_ROOT_USER: minioadmin
      MINIO_ROOT_PASSWORD: minioadmin
    volumes:
      - miniodata:/data

  celery-worker:
    build: .
    command: celery -A app.tasks worker --loglevel=info
    environment:
      - DATABASE_URL=postgresql+asyncpg://visear:visear@db:5432/visear
      - REDIS_URL=redis://redis:6379
    depends_on:
      - db
      - redis

volumes:
  pgdata:
  miniodata:
```

---

## Analytics Strategy

### What to Track

| Category                 | Metrics                                        | Purpose                  |
| ------------------------ | ---------------------------------------------- | ------------------------ |
| **Recognition accuracy** | Confidence scores, top-k accuracy              | Model quality monitoring |
| **Latency**              | Per-stage timing (MediaPipe, ONNX, TTS, total) | Performance optimization |
| **Usage patterns**       | Active modes, session duration, signs/session  | Product decisions        |
| **Error rates**          | Failed recognitions, dropped frames, crashes   | Reliability              |
| **User corrections**     | Predicted vs actual sign corrections           | Retraining data          |
| **Platform stats**       | OS, GPU type, CPU type, RAM                    | Compatibility planning   |
| **Model versions**       | Which model versions are in use                | Rollout monitoring       |

### Privacy Considerations

- No video or audio is ever transmitted to the server
- Landmark data in feedback is anonymized (no video reconstruction possible)
- Client IDs are random UUIDs, not tied to personal identity
- Analytics are opt-in with clear user consent
- All data encrypted in transit (TLS) and at rest
- GDPR-compliant: user can request data deletion
- Telemetry can be fully disabled in app settings

### Analytics Dashboard (Admin)

The Grafana dashboard shows:

```
┌──────────────────────────────────────────────────────────┐
│  Visear Analytics Dashboard                              │
├──────────────────────────┬───────────────────────────────┤
│  Active Users (24h)      │  Avg Recognition Accuracy     │
│  ████████████ 1,234      │  ████████████████ 87.3%       │
├──────────────────────────┼───────────────────────────────┤
│  Avg Latency (ms)        │  Signs Recognized (24h)       │
│  ██████ 48ms             │  ████████████████ 45,678      │
├──────────────────────────┴───────────────────────────────┤
│  Top Misclassifications                                  │
│  THANK → PLEASE  (142 corrections)                       │
│  WANT  → NEED    (89 corrections)                        │
│  HELP  → ASSIST  (67 corrections)                        │
├──────────────────────────────────────────────────────────┤
│  Latency Distribution              Mode Usage            │
│  [histogram chart]                  [pie chart]          │
├──────────────────────────────────────────────────────────┤
│  Model Version Adoption            Platform Distribution │
│  v1.2.0: 78%  v1.1.0: 22%        Win: 65% Mac: 25%       │
└──────────────────────────────────────────────────────────┘
```

---

## Client-Side API Integration (C++)

The desktop app communicates with the backend via a lightweight REST client:

```cpp
// src/network/APIClient.h
#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class APIClient {
public:
    explicit APIClient(const std::string& base_url);

    // Model management
    std::optional<json> checkModelUpdate(const std::string& model_name,
                                          const std::string& current_version);
    bool downloadModel(const std::string& url, const std::string& output_path);

    // Analytics (fire-and-forget, non-blocking)
    void sendEvent(const std::string& event_type, const json& payload);
    void sendSessionSummary(const json& summary);

    // Feedback
    void sendCorrection(const std::string& predicted,
                        const std::string& actual,
                        const std::vector<float>& landmarks);

    // Dictionary
    std::optional<json> searchDictionary(const std::string& query);

    // Health
    bool isServerReachable();

private:
    std::string base_url_;
    std::string client_id_;  // Generated UUID, persisted locally

    // Non-blocking HTTP using cpr library
    void asyncPost(const std::string& endpoint, const json& body);
};
```

The API client runs all telemetry calls asynchronously so they never block the translation pipeline. If the server is unreachable, events are queued locally and sent when connectivity is restored.
