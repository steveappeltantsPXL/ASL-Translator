# 11 — GitHub Repository Administration Reference

> **Audience:** Whoever has Admin access to the GitHub repository.
> **Purpose:** A single reference for all repo settings that must be configured
> manually in the GitHub UI (these cannot be committed as code).

---

## Table of Contents

1. [Branch Protection Rules](#1-branch-protection-rules)
2. [Team Permissions](#2-team-permissions)
3. [Secrets & Environment Variables](#3-secrets--environment-variables)
4. [Dependabot & Security Alerts](#4-dependabot--security-alerts)
5. [Releases & Tags](#5-releases--tags)
6. [Release Branch Lifecycle](#6-release-branch-lifecycle)
7. [Branch Strategy Quick Reference](#7-branch-strategy-quick-reference)

---

## 1. Branch Protection Rules

Navigate to: **Settings → Branches → Add branch ruleset** (or "Add rule" in the
classic UI).

### `main` — Production Branch

| Setting | Value |
|---------|-------|
| Require a pull request before merging | **Enabled** |
| Required approvals | **1** (increase to 2 for critical projects) |
| Dismiss stale reviews on new push | **Enabled** |
| Require status checks to pass | **Enabled** (add CI jobs when configured) |
| Require branches to be up to date | **Enabled** |
| Restrict pushes that create matching refs | **Enabled** (Admins only) |
| Allow force pushes | **Disabled** |
| Allow deletions | **Disabled** |

### `develop` — Integration Branch

| Setting | Value |
|---------|-------|
| Require a pull request before merging | **Enabled** |
| Required approvals | **1** |
| Dismiss stale reviews on new push | **Enabled** |
| Require status checks to pass | **Enabled** |
| Require branches to be up to date | **Enabled** |
| Allow force pushes | **Disabled** |
| Allow deletions | **Disabled** |

> **Note:** `release/v*` branches do **not** need permanent protection rules
> because they are short-lived. Treat them as you would a feature branch —
> PRs required to merge into them, and they are deleted after the release is tagged.

---

## 2. Team Permissions

Navigate to: **Settings → Collaborators and teams**

| Role | Who | GitHub Permission Level |
|------|-----|------------------------|
| **Admin** | Project lead / repo owner | Admin |
| **Maintainer** | Senior contributors who merge PRs | Maintain |
| **Contributor** | Active team members writing code | Write |
| **Viewer** | Stakeholders, advisors, reviewers | Read |

**Maintain** vs **Write:**
- `Maintain` can manage issues/PRs, push to non-protected branches, and manage
  releases — but cannot change repo settings or team access.
- `Write` can push to non-protected branches and create PRs but cannot merge
  without approval or manage releases.

---

## 3. Secrets & Environment Variables

Navigate to: **Settings → Secrets and variables → Actions**

### What to Store in GitHub Secrets

Never hardcode API keys, tokens, or model endpoint URLs in source code or
committed config files. Store them as **repository secrets** (encrypted,
not visible after entry):

| Secret Name | Description |
|-------------|-------------|
| `MEDIAPIPE_MODEL_URL` | URL to the hosted gesture recognizer model (if remote) |
| `CODECOV_TOKEN` | Code coverage upload token (if using Codecov) |
| `RELEASE_SIGNING_KEY` | GPG key for signing release artifacts (future use) |

### Rules

- **Never commit `.env` files.** Add them to `.gitignore`.
- **Never log secret values** in CI output (GitHub masks known secrets
  automatically, but avoid echoing them explicitly).
- **Rotate secrets** if a repository is ever made public or a team member
  with access leaves.
- Use **environment-scoped secrets** (`Settings → Environments`) for staging
  vs production separation once CI/CD is configured.

---

## 4. Dependabot & Security Alerts

Navigate to: **Settings → Code security and analysis**

| Feature | Recommended Setting |
|---------|-------------------|
| Dependency graph | **Enabled** |
| Dependabot alerts | **Enabled** |
| Dependabot security updates | **Enabled** (auto-PRs for vulnerable deps) |
| Secret scanning | **Enabled** |
| Code scanning (CodeQL) | **Enable when CI is configured** |

### Triaging Dependabot PRs

1. Review the Dependabot PR — check the changelog and whether the update is
   breaking.
2. Merge if tests pass and no breaking changes.
3. For breaking updates, open a separate tracking issue and update manually
   when ready.
4. Dismiss alerts only if a vulnerability genuinely does not affect this
   project's usage — always add a dismissal reason.

---

## 5. Releases & Tags

### Tagging Convention

Use **semantic versioning**: `vMAJOR.MINOR.PATCH`

| Segment | When to increment |
|---------|------------------|
| `MAJOR` | Breaking changes or complete redesigns |
| `MINOR` | New features, backwards-compatible |
| `PATCH` | Bug fixes, minor tweaks |

Examples: `v0.1.0`, `v0.2.0`, `v1.0.0`, `v1.0.1`

### Creating a Release

1. Merge the `release/vX.Y.Z` branch into `main` (via PR with required review).
2. Tag `main` at the merge commit:
   ```bash
   git checkout main
   git pull origin main
   git tag -a v0.1.0 -m "Release v0.1.0 — initial gesture recognition MVP"
   git push origin v0.1.0
   ```
3. Create a **GitHub Release** from the tag:
   - Navigate to **Releases → Draft a new release**
   - Select the tag
   - Write release notes (list features, fixes, known issues)
   - Attach any pre-built binaries or model files if distributing them
   - Publish

### GitHub Releases vs Raw Tags

| | GitHub Release | Raw Tag |
|---|---|---|
| Visible on Releases page | Yes | No |
| Supports attachments | Yes | No |
| Release notes | Yes | Tag message only |
| **Use for** | User-facing versions | Internal bookmarks |

Always use **GitHub Releases** for versions users will download or reference.

---

## 6. Release Branch Lifecycle

```
develop ──┬──────────────────────────────────────────> (ongoing)
          │
          └── release/v0.1.0 (cut here for stabilization)
                   │
                   │  [QA, last-minute bugfixes only — no new features]
                   │
                   ├──> PR → main  (merge + tag v0.1.0)
                   │
                   └──> PR → develop  (back-merge fixes)
                             │
                             └── release/v0.1.0 deleted
```

### Step-by-Step

1. **Cut the release branch** from `develop` when feature-complete:
   ```bash
   git checkout develop && git pull origin develop
   git checkout -b release/v0.1.0
   git push -u origin release/v0.1.0
   ```

2. **Stabilize** — only bugfixes, documentation, version bumps. No new features.
   All changes go in via PR to `release/v0.1.0`.

3. **Merge to `main`** — open a PR from `release/v0.1.0` → `main`. Requires
   review and passing CI.

4. **Tag the release** on `main` (see [Section 5](#5-releases--tags)).

5. **Back-merge to `develop`** — open a PR from `release/v0.1.0` → `develop`
   (or from `main` → `develop`) to carry any stabilization fixes back.

6. **Delete the release branch** after both merges are complete:
   ```bash
   git push origin --delete release/v0.1.0
   ```

> Release branches serve as a **staging environment**: QA happens here,
> stakeholder demos happen here. `main` only receives code that has passed
> this stabilization phase.

---

## 7. Branch Strategy Quick Reference

```
main          ← protected, production-stable, tag releases here
develop       ← protected, integration, all features merge here first
feature/*     ← short-lived, branch from develop, merge via PR
hotfix/*      ← branch from main, merge to main + develop
release/v*    ← cut from develop, stabilize, merge to main + develop, delete
```

### Hotfix Flow

A hotfix is a critical fix that cannot wait for the next planned release:

```bash
git checkout main && git pull origin main
git checkout -b hotfix/fix-crash-on-startup
# ... fix the bug ...
git push -u origin hotfix/fix-crash-on-startup
# Open PR → main (emergency review)
# After merge: tag a patch release (e.g. v0.1.1)
# Open PR → develop to back-merge the fix
```

### Who Can Merge Where

| Target Branch | Who Can Merge |
|---------------|---------------|
| `main` | Admin or Maintainer, after PR approval + CI |
| `develop` | Maintainer or Write (via PR), after PR approval |
| `feature/*` | Author (push directly, or PR if collaborating) |
| `release/v*` | Maintainer only (bugfixes and docs only) |

---

*Last updated: 2026-02-20*
