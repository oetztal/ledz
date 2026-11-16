## Why

The pre-commit gate and CI validate the specs with the OpenSpec CLI pinned to `1.15.0` by `OPENSPEC_VERSION` in `scripts/validate-openspec.sh`. OpenSpec 2.0 is the current major, and this pin is the one thing that makes the local gate and the CI runner agree on what a valid spec is. Holding at `1.15.0` keeps the project validating against a superseded major: it misses the stricter checks later 1.x releases added, and it drifts from the tool the OpenSpec workflows are run with. Issue #25 asks for the pin to move to 2.0.

## What Changes

- **The pin moves to the current major.** `scripts/validate-openspec.sh` defaults `OPENSPEC_VERSION` to `2.0.0` instead of `1.15.0`. It stays an exact pin rather than a range, for the reason the script's own comment gives: a different major can silently validate or invalidate specs in ways the two environments disagree on.
- **The raised Node.js floor is documented.** OpenSpec 1.16.0 raised the npm package's engine requirement to Node.js `>=22.12.0` (from `>=20.19.0` in 1.15.0), and the script installs through `npx`, so it runs on whatever Node the machine has. `AGENTS.md` and the "tooling assumed on PATH" comment in `.pre-commit-config.yaml` state the floor. **BREAKING** for a contributor whose default Node is older than 22.12.0: the pre-commit gate will refuse to run until they upgrade Node or use the standalone `openspec` binary.
- **The OpenSpec 2.0 "skills only" migration is a no-op here.** 2.0 stops writing `/opsx:*` command files and installs skills once under `.agents/skills/`, and `openspec update` moves a project over. This repository has never run `openspec init`/`update` — it has no generated command files, no skill copies and no `openspec/AGENTS.md`, and it uses OpenSpec for `validate` only — so there is nothing to move and no `openspec update` run is needed.
- **The existing specs are re-checked under the stricter validator.** The 1.16.0 release turned several validator findings into `--strict` failures (a scenario with no WHEN/GIVEN or no THEN step, an over-long requirement, unrecognized `.openspec.yaml` keys, an unrecognized delta section, and text a delta would drop). The change records the result of running `validate --all --strict` with the 2.0.0 CLI and fixes anything it flags.

## Capabilities

### New Capabilities
- *(none)*

### Modified Capabilities
- *(none)* — this is a build/CI tooling change with no spec-level behaviour change, so `.openspec.yaml` sets `skip_specs: true` instead of inventing a requirement.

## Impact

- `scripts/validate-openspec.sh` — the `OPENSPEC_VERSION` default (and its comment).
- `AGENTS.md`, `.pre-commit-config.yaml` — document the Node.js `>=22.12.0` requirement for the validation gate.
- `.github/workflows/test.yml` — verified only: the GitHub-hosted Ubuntu image already ships Node.js 22.23.4 / 24.x, both above the floor, so no workflow change is expected.
- No firmware, web UI, NVS or HTTP behaviour changes; no `openspec/specs/**` requirement changes.
