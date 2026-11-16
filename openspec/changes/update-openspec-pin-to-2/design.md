# Design

## Context

See `proposal.md` — Why. The relevant current state:

- `scripts/validate-openspec.sh` is the single entry point for validation. It runs `npx -y "@fission-ai/openspec@${OPENSPEC_VERSION}" validate --all --strict --no-interactive` from the repository root, with `OPENSPEC_VERSION` defaulting to `1.15.0` and overridable from the environment. Its comment records why the pin is exact: "A different major can silently validate or invalidate specs in ways the two environments disagree on."
- Two callers run that script: the `openspec-validate` pre-commit hook (`language: script`, `files: ^openspec/`) and `.github/workflows/test.yml`, which runs the same hooks through `pre-commit/action@v3.0.1` before the PlatformIO build. Neither caller installs Node.js itself, so the script's `npx` uses the machine's or runner's default Node.
- `npm view` confirms the engine boundary: `@fission-ai/openspec@1.15.0` requires Node `>=20.19.0`; `1.16.0` and `2.0.0` require `>=22.12.0`.
- The repository has no `openspec init` output — no `.opencode/`, `.agents/`, `.claude/` or `.github` command files, and no `openspec/AGENTS.md`. OpenSpec is used for `validate` only; the propose/apply/archive workflows run against the `openspec/` tree.
- Running the pinned command with the 2.0.0 CLI (`openspec validate --all --strict --no-interactive`) passes all 14 main specs and reports only the change under construction, so the stricter 1.16+/2.0 rules do not invalidate any committed spec.
- OpenSpec 2.0's headline change is delivery: skills only, installed once under `.agents/skills/`, replacing the `/opsx:*` command files. It applies to a project through `openspec update`, which this repository has no reason to run.

## Goals / Non-Goals

**Goals:**
- Local pre-commit and CI validate with the same OpenSpec 2.0 CLI.
- The gate's Node.js floor is stated where contributors and the CI comment already state the gate's prerequisites.
- Confirm that 2.0.0 does not invalidate the committed specs, so the bump is a one-line change with no follow-on spec edits.

**Non-Goals:**
- Adopting OpenSpec skills or command files in the repository (there is nothing to migrate to).
- Changing how changes are created, applied or archived; only the `validate` invocation is affected.
- Moving the script off `npx` to the standalone `openspec` binary, or to a different validator.
- Adding a CI step that pins Node.js, or an automated version-drift check such as `openspec version --check`; the pin stays a deliberate manual bump.
- Adding or editing a spec requirement for this change (see Decision 4).

## Decisions

### 1. Pin `2.0.0` exactly, not a range or `latest`

`validate --all --strict` is the gate; a floating `^2` or `latest` would let a new release tighten validation between a contributor's run and CI's run, which is exactly the failure mode the script's comment warns about. An exact pin keeps the two environments on the same parser and the same rules. The alternative — `latest` with automatic pickup of fixes — was rejected because an unpinned validator can turn a green branch red (or vice versa) with no code change.

### 2. Keep `npx`, accept the raised Node floor, and document it

The script installs the CLI on demand through `npx`, which is what makes the pre-commit hook self-contained: `language: script` runs the entry directly and cannot install an OS-specific binary. The cost is that `npx` inherits the machine's Node, and 2.0's package needs Node `>=22.12.0`. That is a real but small cost: the GitHub-hosted Ubuntu images ship Node 22.23.4 (and 24.x) today, comfortably above the floor, and any developer machine that can run the rest of this toolchain can install a current Node.

The alternative was to switch to the standalone `openspec` binary, which is dependency-free and is upstream's default install since 1.15.0. It was rejected as out of scope: it would add platform detection plus a download/cache step to a hook that currently does nothing but call `npx`, and issue #25 asks for a version bump, not an install-strategy change. It remains the documented escape hatch for anyone who cannot move off an older Node.

A second alternative — adding `actions/setup-node` to `test.yml` to pin the runner's Node — was also rejected. The pinned CLI major is what makes validation deterministic; the Node version only has to satisfy the engine floor, and the runner's job is to satisfy it, which the current image does. Pinning Node in CI would add a dependency that guards against a runner regression the project cannot yet observe.

### 3. CI compatibility is checked, not assumed

Rather than adding a workflow step, the change verifies the runner's Node against the floor. The current Ubuntu 24.04 image manifest lists Node.js 22.23.4 as the default and 24.22.2 as a cached version, both `>=22.12.0`; the announced move of `ubuntu-latest` to Ubuntu 26.04 does not lower it. If a future image ever drops below the floor, the failure is the same `npx` engine error a local contributor would see, and the fix (pin Node in the workflow, or switch to the standalone binary) is recorded here and in the proposal.

### 4. `skip_specs: true` — no spec delta

Specs describe system behaviour. Bumping the tool that checks the specs changes how the repository is validated, not the firmware, web UI, HTTP API or NVS behaviour the 14 capabilities specify, so no requirement changes and `openspec validate` would reject an empty `specs/` folder. The change opts out explicitly with `skip_specs: true` in `.openspec.yaml`, which is the validator's sanctioned marker for a pure tooling change. Inventing a requirement to satisfy the validator was rejected outright.

### 5. No `openspec update` and no skills migration

The 2.0 delivery change replaces generated `/opsx:*` command files with shared skills under `.agents/skills/`, applied by `openspec update`. This repository has no such generated files — it never ran `openspec init` — so the migration is a no-op. Running `openspec update` anyway would create integration files the project does not use and has deliberately kept out of the checkout; that stays a non-goal. The change records the check (`git ls-files` for `opsx`/`.agents`/`.opencode` paths returns nothing) so the reasoning is auditable.

### 6. Verify under 2.0.0 rather than back-filling 1.16.x behaviour

The bump crosses 1.16.0, which added `--strict`-failing checks (scenario step coverage, 500-character requirement descriptions, unknown `.openspec.yaml` keys, unrecognized delta sections, dropped delta text). The change verifies the committed specs against 2.0.0 and stops there. It does not pre-emptively rewrite specs to satisfy rules that only future deltas could trip; if a future change trips one, that change fixes it. This keeps the diff a one-line pin plus documentation.

## Risks / Trade-offs

- **The validator is stricter under 2.0 and a future delta trips a new strict rule** → The 1.16.0 rules are the same ones the 2.0.0 run already enforces against everything committed today; the change verifies the current tree passes, and a future delta fails with the rule's own message rather than silently.
- **A contributor on Node < 22.12.0 can no longer run the pre-commit gate** → Documented in `AGENTS.md` and the pre-commit comment; the error is immediate and self-describing (`EBADENGINE`), and upgrading Node or installing the standalone binary clears it. Marked **BREAKING** in the proposal so it is not a surprise.
- **A future GitHub runner image drops below the Node floor** → The pinned CLI version is independent of Node; only the engine floor matters, and the current and next Ubuntu images exceed it. The mitigation (pin Node in the workflow or move to the standalone binary) is recorded in Decision 3.
- **`npx` still needs network access on a cold cache** → Unchanged by this change; it is already how the gate works, and a warm `npx` cache avoids it.
- **The pin drifts again after the next OpenSpec major** → The script's comment explains the intent; this change does not add automation to track upstream, deliberately (Non-Goals). A follow-up can add a scheduled drift check if that becomes worth it.

## Migration Plan

1. Bump `OPENSPEC_VERSION` to `2.0.0` in `scripts/validate-openspec.sh` and update its comment.
2. Update the Node.js floor in `AGENTS.md` and the `.pre-commit-config.yaml` tooling comment.
3. Confirm the runner's Node (`actions/runner-images` manifest) satisfies `>=22.12.0`; no workflow change.
4. Run the pinned command (`openspec validate --all --strict --no-interactive`) and the pre-commit `openspec-validate` hook; both must pass.

Rollback is a single revert: restore `OPENSPEC_VERSION` to `1.15.0`. The documentation can stay or be reverted with it; there is no persisted state, so rollback is not order-dependent.

## Open Questions

None. The reviewer (wuan) approved the change as specified on 2026-11-16. The
approved decisions are the exact `2.0.0` pin over a floating `latest`/`^2`
(decision 1), keeping `npx` and documenting the raised Node.js `>=22.12.0` floor
in `AGENTS.md` and the pre-commit comment (decision 2), `skip_specs: true` for a
pure tooling change with no spec delta (decision 4), and treating the 2.0 skills
migration as a no-op with no `openspec update` run (decision 5).

On a second review the same day, wuan approved the frozen artifacts and directed
that the change be implemented ("Implement the change"), so the scope above is
final and no further propose-side revisions were requested. Implementation is
the apply-stage work captured in `tasks.md`.

The committed tree still validates under both `1.15.0` and `2.0.0` with
`validate --all --strict`: all 14 specs pass, so the bump stays a one-line pin
change plus documentation and no spec has to be edited.
