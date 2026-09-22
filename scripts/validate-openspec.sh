#!/bin/bash
set -e

# Pinned so local pre-commit and any CI runner validate with the same CLI
# major. A different major can silently validate or invalidate specs in ways
# the two environments disagree on.
OPENSPEC_VERSION="${OPENSPEC_VERSION:-1.13.1}"

# openspec/ lives at the repository root; run from there so the CLI finds it.
cd "$(dirname "$0")/.."

npx -y "@fission-ai/openspec@${OPENSPEC_VERSION}" validate --all --strict --no-interactive
