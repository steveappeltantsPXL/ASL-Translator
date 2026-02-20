# Contributing to Visear ASL Translator

Thank you for helping make real-time sign language translation more accessible!
Please read this guide before opening issues or submitting pull requests.

---

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [Getting Started](#getting-started)
3. [Branching Strategy](#branching-strategy)
4. [Commit Messages](#commit-messages)
5. [Opening a Pull Request](#opening-a-pull-request)
6. [Code Style](#code-style)
7. [Review Process](#review-process)
8. [Contributor License Agreement (CLA)](#contributor-license-agreement)

---

## Code of Conduct

Be respectful, constructive, and inclusive. Harassment of any kind is not tolerated.

---

## Getting Started

1. **Fork** the repository and clone your fork locally.
2. Install dependencies as described in `README.md`.
3. Build the project and verify it runs before making changes.
4. Create a branch for your work (see [Branching Strategy](#branching-strategy)).

---

## Branching Strategy

| Branch | Purpose |
|--------|---------|
| `main` | Production-stable. Never commit directly here. |
| `develop` | Integration branch. All features merge here first. |
| `feature/<name>` | New feature or improvement. Branch from `develop`. |
| `hotfix/<name>` | Critical fix against `main`. Merge back to `main` + `develop`. |
| `release/vX.Y.Z` | Stabilization branch cut from `develop` before a release. |

```bash
# Start a new feature
git checkout develop
git pull origin develop
git checkout -b feature/my-feature-name

# When done, push and open a PR targeting develop
git push -u origin feature/my-feature-name
```

Branch names should be lowercase, hyphen-separated, and descriptive:
`feature/two-hand-gesture-detection`, `hotfix/camera-init-crash`.

---

## Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short summary>

[optional body]

[optional footer: Closes #123]
```

**Types:** `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `build`, `ci`

**Examples:**
```
feat(model): add two-hand gesture classifier
fix(camera): resolve crash on init with no webcam
docs(readme): update build instructions for Windows
```

- Keep the summary line under 72 characters.
- Use present tense ("add feature" not "added feature").
- Reference related issues in the footer (`Closes #42`).

---

## Opening a Pull Request

1. Ensure your branch is up to date with `develop`:
   ```bash
   git fetch origin
   git rebase origin/develop
   ```
2. Open a PR targeting the `develop` branch (not `main`).
3. Fill out the PR template completely.
4. Link any related issues in the description.
5. Request at least one reviewer.
6. Address all review comments before requesting re-review.

PRs that skip the template, target `main` directly (without going through `develop`
first), or lack a test plan will be asked to update before review begins.

---

## Code Style

- **C++17** standard.
- Follow the existing code structure and naming conventions already in the project.
- Keep functions focused and small; prefer clarity over cleverness.
- No magic numbers — use named constants.
- Do not commit commented-out code or debug `printf`/`std::cout` calls.
- Format code consistently; if the project adopts a `.clang-format` config, run
  `clang-format` before committing.

---

## Review Process

- All PRs require **at least one approving review** before merge.
- CI checks (if configured) must pass.
- Reviewers aim to respond within **2 business days**.
- The author is responsible for resolving conflicts and keeping the branch up to date.
- Maintainers may squash-merge feature branches to keep `develop` history clean.

---

## Contributor License Agreement

Before your first contribution is merged, you must agree to the project's
[Contributor License Agreement](CLA.md).

To sign, add your name, GitHub username, and the date to the CLA file in your
first PR:

```
| Your Name | @your-github-username | YYYY-MM-DD |
```

This ensures the project can remain open-source and that all contributors have
the right to submit their code.
