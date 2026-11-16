#!/bin/bash
set -e

# Pinned so local pre-commit and any CI runner validate with the same CLI
# major. A different major can silently validate or invalidate specs in ways
# the two environments disagree on.
#
# The 2.0 package requires Node.js >=22.12.0 (1.15.0 only needed >=20.19.0),
# and npx runs on whatever Node the machine has; upgrade Node or use the
# standalone `openspec` binary if the gate reports EBADENGINE.
OPENSPEC_VERSION="${OPENSPEC_VERSION:-2.0.0}"

# openspec/ lives at the repository root; run from there so the CLI finds it.
cd "$(dirname "$0")/.."

npx -y "@fission-ai/openspec@${OPENSPEC_VERSION}" validate --all --strict --no-interactive
