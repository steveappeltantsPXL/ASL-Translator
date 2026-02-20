# GitHub Workflow — Visear ASL Translator

**Repository:** https://github.com/steveappeltantsPXL/ASL-Translator

This document covers everything related to using the project with GitHub:
cloning, branching, committing, pull requests, submodules, and day-to-day workflow.

---

## Cloning the Project (New Machine)

```powershell
# Clone the repo
git clone https://github.com/steveappeltantsPXL/ASL-Translator.git
cd ASL-Translator

# Pull the ImGui submodule (and any future submodules)
git submodule update --init --recursive
```

Then follow `docs/BUILD-COMMANDS.md` to install vcpkg, configure, and build.

---

## Branch Strategy

```
main                    ← Stable, always buildable. Direct commits only for
│                         initial setup or hotfixes.
├── develop             ← Integration branch. All features merge here first.
│   ├── feature/camera-capture
│   ├── feature/asl-pipeline
│   ├── feature/onnx-model-loading
│   ├── feature/imgui-panels
│   └── feature/virtual-camera
└── release/v0.1.0      ← Release candidates cut from develop
```

### Rules

| Branch | Who commits | Merges into |
|---|---|---|
| `main` | Nobody directly (PR only) | — |
| `develop` | Nobody directly (PR only) | `main` |
| `feature/*` | You, freely | `develop` via PR |
| `release/*` | Bug fixes only | `main` + back to `develop` |

---

## Daily Development Workflow

### 1. Start a new feature

```powershell
# Always branch from develop, not main
git checkout develop
git pull origin develop

git checkout -b feature/camera-capture
```

### 2. Work and commit

```powershell
# Build and test as you go
cmake --build build --config Debug
.\build\Debug\VisearASLTranslator.exe

# Stage specific files (avoid git add -A to prevent accidental includes)
git add src/capture/CameraCapture.h src/capture/CameraCapture.cpp

# Commit with a clear message
git commit -m "Add CameraCapture wrapper around cv::VideoCapture"
```

### 3. Push and open a Pull Request

```powershell
git push -u origin feature/camera-capture
```

```powershell
# Open PR against develop (not main)
gh pr create --title "Add camera capture" --base develop --body "$(cat <<'EOF'
## Summary
- Wraps cv::VideoCapture with RAII management
- Exposes frame() method returning cv::Mat
- Handles device not found gracefully

## Test plan
- [ ] Camera panel shows live feed on launch
- [ ] App doesn't crash when camera is unplugged
EOF
)"
```

### 4. Merge and clean up

After PR is reviewed and merged:

```powershell
git checkout develop
git pull origin develop

# Delete the feature branch locally and remotely
git branch -d feature/camera-capture
git push origin --delete feature/camera-capture
```

---

## Commit Message Guidelines

```
<short imperative summary, max 72 chars>

<optional body — what and why, not how>
```

**Good examples:**

```
Add ONNX session manager with GPU provider fallback

Load gesture classifier model on startup. Falls back from DirectML
to CPU if no compatible GPU is detected. Logs provider selection.
```

```
Fix SDL window not closing on Alt+F4
```

```
Refactor pipeline manager to use producer-consumer pattern
```

**Avoid:**

```
wip
fix stuff
updated files
changes
```

---

## Submodules

The project uses git submodules for vendored C++ libraries that aren't in vcpkg.

### Currently registered submodules

| Submodule | Branch | Path | Purpose |
|---|---|---|---|
| Dear ImGui | docking | `vendor/imgui` | UI rendering |
| whisper.cpp | main | `vendor/whisper.cpp` | STT *(not yet added)* |
| piper | main | `vendor/piper` | TTS *(not yet added)* |

### Cloning — always run after a fresh clone

```powershell
git submodule update --init --recursive
```

### Adding a new submodule

```powershell
git submodule add -b <branch> <url> vendor/<name>
git add .gitmodules vendor/<name>
git commit -m "Add <name> as vendor submodule"
```

### Updating a submodule to its latest upstream commit

```powershell
git -C vendor/imgui pull origin docking
git add vendor/imgui
git commit -m "Update imgui submodule to latest docking"
```

### After someone else updates a submodule

```powershell
git pull
git submodule update --recursive
```

> **Important:** Never commit changes inside `vendor/imgui/` directly.
> Submodule contents are pinned to a specific commit — update via the commands above.

---

## Keeping Your Fork / Branch Up to Date

```powershell
# Sync develop with remote
git checkout develop
git pull origin develop

# Rebase your feature branch onto latest develop
git checkout feature/my-feature
git rebase develop

# If there are conflicts, resolve them, then:
git rebase --continue
```

---

## Tagging a Release

```powershell
# After merging develop → main
git checkout main
git pull origin main

git tag -a v0.1.0 -m "v0.1.0 — Initial working build: SDL3 + ImGui window"
git push origin v0.1.0
```

---

## Useful `gh` CLI Commands

```powershell
# View open PRs
gh pr list

# Check out a PR locally
gh pr checkout 12

# View repo in browser
gh repo view --web

# View recent commits
gh repo view steveappeltantsPXL/ASL-Translator

# Create an issue
gh issue create --title "Camera capture crashes on device index 1" --body "..."

# List issues
gh issue list
```

---

## What to Never Commit

The `.gitignore` handles most of this automatically, but be aware:

| Item | Reason |
|---|---|
| `build/` | Generated by CMake — reproducible |
| `vcpkg_installed/` | Installed by vcpkg — reproducible |
| `*.exe`, `*.dll`, `*.lib` | Build artifacts |
| `*.pdb` | Debug symbols |
| `imgui.ini` | Runtime UI layout — personal preference |
| `resources/models/*.onnx` | Large binaries — use download script |
| `.env` | Secrets |
| `cmake-output.log` | Temporary build log |

If you accidentally stage something from these categories:

```powershell
git restore --staged <file>
```

---

## Quick Reference

```powershell
# Start work
git checkout develop && git pull
git checkout -b feature/<name>

# Daily
git add <specific files>
git commit -m "Clear description of what changed"

# Share
git push -u origin feature/<name>
gh pr create --base develop

# After merge
git checkout develop && git pull
git branch -d feature/<name>
```
