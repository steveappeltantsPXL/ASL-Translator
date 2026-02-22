# 13 — Claude Code: Rules, Workflows & Project Structure

Quick reference for how Claude Code reads configuration, rules, agents, skills, and hooks.

---

## Default Directory Structure

Everything Claude reads automatically lives in `.claude/` or project root:

```
your-project/
├── CLAUDE.md                      ← Loaded on every session start
├── CLAUDE.local.md                ← Personal overrides (auto-gitignored)
└── .claude/
    ├── CLAUDE.md                  ← Alternative to root CLAUDE.md
    ├── settings.json              ← Team config (committed to git)
    ├── settings.local.json        ← Personal config (gitignored)
    ├── agents/                    ← Project-level subagents
    │   └── my-agent.md
    ├── skills/                    ← Custom slash commands & workflows
    │   └── my-workflow/
    │       └── SKILL.md
    ├── rules/                     ← Modular, scoped instruction files
    │   ├── backend.md
    │   └── frontend/
    │       └── react.md
    ├── hooks/                     ← Hook scripts referenced by settings.json
    │   └── protect-files.sh
    └── .mcp.json                  ← MCP server configuration
```

User-level config (applies to all projects):

```
~/.claude/
├── CLAUDE.md          ← Personal preferences for all projects
├── settings.json      ← Personal defaults
├── agents/            ← Personal subagents
├── skills/            ← Personal slash commands
└── rules/             ← Personal rules
```

---

## Priority Hierarchy (low → high)

| Level    | Location                       | Shareable    |
|----------|--------------------------------|--------------|
| User     | `~/.claude/`                   | No           |
| Project  | `.claude/`                     | Yes (git)    |
| Local    | `.claude/*.local.json`         | No (gitignored) |
| Managed  | System/IT policies             | N/A          |

More specific always wins — project overrides user, local overrides project.

---

## What Loads When

| File / Directory            | When loaded               | Notes |
|-----------------------------|---------------------------|-------|
| `CLAUDE.md` (root or `.claude/`) | Every session start  | Keep concise |
| `.claude/rules/*.md`        | Every session start       | All `.md` files, recursively |
| `.claude/settings.json`     | Every session start       | Permissions, hooks, model |
| `.claude/settings.local.json` | Every session start     | Gitignored automatically |
| `.claude/agents/*.md`       | When Claude delegates     | Not automatic |
| `.claude/skills/*/SKILL.md` | On invocation or startup  | `/skill-name` or Claude match |
| `.claude/.mcp.json`         | Every session start       | MCP server list |
| Subdirectory `CLAUDE.md`    | On-demand                 | Loads when Claude reads files there |
| `~/.claude/CLAUDE.md`       | Every session start       | Global personal memory |

---

## CLAUDE.md — Project Memory

Run `/init` to generate it from your actual codebase.

**Include** (things Claude can't guess):
- Build and test commands
- Architectural decisions specific to your project
- Non-obvious gotchas or constraints
- Preferred tools and libraries
- Branch naming and PR conventions

**Exclude** (noise):
- File-by-file descriptions (Claude can read them)
- Standard language conventions
- Long tutorials or explanations
- Anything that changes frequently

**Example:**
```markdown
# Build
- cmake --build cmake-build-debug-msvc --config Debug

# Rules
- RAII only — no raw new/delete
- #ifdef _WIN32 around all Win32 headers
- windows.h must precede GL/gl.h on MSVC

# Architecture
- ImGui render loop: NewFrame → draw calls → Render → SwapWindow
- Desktop layout: toolbar 40px top, captions 80px bottom, panels fill middle
```

---

## .claude/rules/ — Modular Instructions

Split a large CLAUDE.md into topic files. All `.md` files here load automatically.

```
.claude/rules/
├── cpp-style.md       ← C++20, RAII, MSVC specifics
├── imgui-layout.md    ← Panel layout constants and rules
├── cmake.md           ← vcpkg + CMakeLists conventions
└── ml/
    └── onnx.md        ← ONNX Runtime inference rules
```

### Path-Scoped Rules (YAML frontmatter)

A rule file can activate only for matching files:

```yaml
---
paths:
  - "src/**/*.cpp"
  - "src/**/*.h"
---

# C++ Rules

- No raw new/delete — use std::unique_ptr or stack objects
- All Win32 headers behind #ifdef _WIN32
```

Supported glob patterns: `**/*.ts`, `src/**`, `*.md`, `{src,lib}/**/*.{ts,tsx}`

---

## .claude/skills/ — Slash Commands & Workflows

Skills become `/skill-name` commands you or Claude can invoke.

```
.claude/skills/
└── fix-issue/
    ├── SKILL.md           ← Required, defines the skill
    └── scripts/           ← Supporting files (optional)
        └── validate.sh
```

### SKILL.md Format

```yaml
---
name: fix-issue
description: Diagnose and fix a GitHub issue by number
disable-model-invocation: false    # Claude can invoke automatically
user-invocable: true               # You can invoke with /fix-issue
allowed-tools: Read, Grep, Bash, Edit
---

Given issue $ARGUMENTS:
1. Read the issue with `gh issue view $ARGUMENTS`
2. Find relevant source files
3. Implement a fix
4. Write a test
```

### Invocation Control

| Setting | You invoke | Claude invokes |
|---------|-----------|---------------|
| defaults | `/skill-name` | Yes, automatically |
| `disable-model-invocation: true` | `/skill-name` | Never |
| `user-invocable: false` | Never | Yes, automatically |

### Dynamic Context Injection

Use `!` prefix to run a command and inject its output before Claude sees the skill:

```yaml
## Current State
- Diff: !`gh pr diff`
- Open issues: !`gh issue list --state open`
```

---

## .claude/agents/ — Specialized Subagents

Define agents Claude can delegate complex subtasks to.

```markdown
---
name: cpp-advisor
description: Senior C++20 engineer for MSVC/CMake/vcpkg guidance
tools: Read, Grep, Glob, Bash
model: sonnet
---

You are a C++20 specialist. You enforce RAII, const-correctness,
and catch MSVC-specific issues at /W4...
```

Claude decides when to delegate based on the task. You can also ask explicitly:
> "Use the cpp-advisor agent to review this file."

**Project agents** live in `.claude/agents/` (committed to git).
**User agents** live in `~/.claude/agents/` (all projects, not committed).

> Note: Project agents don't appear in `/agents` UI but are active and usable.

---

## .claude/settings.json — Permissions & Hooks

### Permissions

```json
{
  "permissions": {
    "allow": [
      "Bash(cmake --build *)",
      "Bash(git commit *)",
      "Bash(powershell:*)"
    ],
    "deny": [
      "Bash(rm -rf *)",
      "Write(.env)"
    ]
  }
}
```

### Hooks

Run shell commands automatically at lifecycle events:

```json
{
  "hooks": {
    "PostToolUse": [
      {
        "matcher": "Edit|Write",
        "hooks": [
          {
            "type": "command",
            "command": ".claude/hooks/format.sh"
          }
        ]
      }
    ]
  }
}
```

| Event | When | Common Use |
|-------|------|------------|
| `SessionStart` | Session begins | Inject reminders, set env vars |
| `PreToolUse` | Before a tool runs | Block dangerous commands |
| `PostToolUse` | After tool succeeds | Auto-format, run tests |
| `PermissionRequest` | Permission prompt | Auto-approve safe patterns |
| `Notification` | Claude needs attention | Desktop notifications |

### Hook Exit Codes

| Exit code | Effect |
|-----------|--------|
| `0` | Allow the action |
| `2` | Block the action |
| other | Log error, allow |

**Example hook** — protect sensitive files:

```bash
#!/bin/bash
INPUT=$(cat)
FILE=$(echo "$INPUT" | jq -r '.tool_input.file_path // empty')
for protected in ".env" "secrets.json"; do
  if [[ "$FILE" == *"$protected"* ]]; then
    echo "Blocked: $FILE is protected" >&2
    exit 2
  fi
done
exit 0
```

---

## .claude/.mcp.json — External Tool Servers

```json
{
  "mcpServers": {
    "github": {
      "type": "stdio",
      "command": "npx",
      "args": ["@anthropic-ai/github-mcp"]
    }
  }
}
```

---

## Recommended Layout for This Project

```
Visear-ASL-Translator/
├── CLAUDE.md                          ← Build commands, architecture, gotchas
└── .claude/
    ├── settings.json                  ← Allowed cmake/powershell commands
    ├── settings.local.json            ← Personal overrides (gitignored)
    ├── agents/
    │   ├── cpp-advisor.md             ← C++20/MSVC/CMake specialist
    │   └── imgui-sdl-advisor.md       ← ImGui + SDL3 UI specialist
    ├── skills/
    │   └── rebuild/
    │       └── SKILL.md               ← /rebuild: kill process, cmake build, run
    └── rules/
        ├── cpp-style.md               ← RAII, Win32 guards, MSVC flags
        ├── imgui-layout.md            ← Panel constants, Push/Pop rules
        └── cmake.md                   ← vcpkg.json + CMakeLists conventions
```

---

## Quick Cheatsheet

| Goal | What to use |
|------|-------------|
| Always-on project instructions | `CLAUDE.md` |
| Modular topic-specific rules | `.claude/rules/*.md` |
| Rules that apply only to certain files | `.claude/rules/*.md` with `paths:` frontmatter |
| Custom `/command` you invoke | `.claude/skills/name/SKILL.md` |
| Specialized knowledge for delegation | `.claude/agents/name.md` |
| Automatic post-edit formatting | Hook in `settings.json` |
| Personal preferences (not committed) | `~/.claude/CLAUDE.md` or `CLAUDE.local.md` |
| Per-machine build paths / env vars | `.claude/settings.local.json` |
