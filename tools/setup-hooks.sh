#!/usr/bin/env bash
# Configure Git to use the project's .githooks/ directory.
# Run once after cloning: bash tools/setup-hooks.sh

set -e

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

git config core.hooksPath .githooks
chmod +x .githooks/*

echo "Git hooks activated (.githooks/)"
echo "  pre-commit  — auto-formats staged C++ files with clang-format"
echo "  commit-msg  — enforces conventional commit messages"
echo "  pre-push    — verifies the project builds before pushing"
