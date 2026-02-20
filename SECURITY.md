# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| `main` (latest) | Yes |
| `develop` | Yes (pre-release) |
| Older tagged releases | No — please update to the latest release |

---

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

To report a security issue privately:

1. Go to the repository's **Security** tab on GitHub.
2. Click **"Report a vulnerability"** (GitHub Private Security Advisory).
3. Fill in a clear description, steps to reproduce, and impact assessment.

Alternatively, contact the maintainer directly via the email listed in their
GitHub profile with the subject line: `[SECURITY] Visear ASL Translator`.

---

## Response SLA

| Step | Target Time |
|------|-------------|
| Initial acknowledgment | Within **72 hours** of report |
| Severity assessment | Within **5 business days** |
| Fix or mitigation | Within **30 days** for high/critical; best-effort for low/medium |
| Public disclosure | Coordinated with reporter after fix is released |

We follow a **coordinated disclosure** model. Please give us a reasonable window
to patch before publishing details publicly.

---

## Scope

This project is a **local desktop application** that performs real-time ASL
gesture recognition. Common areas of concern include:

- Loading of external model files or configuration that could be tampered with
- Camera/video stream handling
- Any networked features (API calls, telemetry) added in future versions

Out of scope: vulnerabilities in third-party libraries (MediaPipe, OpenCV,
ImGui, etc.) — please report those to the respective upstream projects.

---

## Thank You

We appreciate responsible disclosure. Security researchers who report valid
vulnerabilities will be credited in the release notes (unless they prefer
to remain anonymous).
