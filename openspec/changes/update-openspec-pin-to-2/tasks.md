## 1. Bump the pin

- [ ] 1.1 `scripts/validate-openspec.sh`: change the default `OPENSPEC_VERSION="${OPENSPEC_VERSION:-1.15.0}"` to `2.0.0` and extend the surrounding comment to name the Node.js `>=22.12.0` floor the 2.0 package needs. Verify with `grep -n 'OPENSPEC_VERSION' scripts/validate-openspec.sh`, which shows the default `2.0.0` and the comment, and `bash -n scripts/validate-openspec.sh`, which exits 0.
- [ ] 1.2 Make the bump a single-purpose commit so it can be reverted on its own: `build(deps): pin OpenSpec CLI to 2.0.0`. Verify with `git log --oneline -1` and `git show --stat` showing only `scripts/validate-openspec.sh`.

## 2. State the Node.js floor

- [ ] 2.1 Confirm the engine requirement the docs must state: run `npm view @fission-ai/openspec@2.0.0 engines` and expect `{ node: '>=22.12.0' }`, and `npm view @fission-ai/openspec@1.15.0 engines` and expect `{ node: '>=20.19.0' }` so the wording can say the floor rose from the old pin.
- [ ] 2.2 `AGENTS.md`: in the formatting/pre-commit paragraph that currently ends with "OpenSpec validation is pinned via `OPENSPEC_VERSION` in `scripts/validate-openspec.sh`", add that the gate needs Node.js `>=22.12.0` (the 2.0 package's engine floor). Verify by reading the paragraph back; it names both the pin and the Node floor.
- [ ] 2.3 `.pre-commit-config.yaml`: extend the "Tooling assumed on PATH" comment's `openspec CLI (auto-installed by scripts/validate-openspec.sh)` line with the Node.js `>=22.12.0` requirement. Verify by reading the comment block; `pre-commit validate-config` (or `pre-commit run --all-files` startup) still parses the file.
- [ ] 2.4 Verify the CI runner already clears the floor: read the `actions/runner-images` Ubuntu 24.04 manifest and confirm the default Node.js (22.23.4 at the time of writing) and cached 24.x are `>=22.12.0`. Verify by recording the observed versions in the PR description. *Only if* the runner is below the floor, add `actions/setup-node@v7` with `node-version: '22'` before the "Run pre-commit hooks" step in `.github/workflows/test.yml`; otherwise make no workflow change and note why.

## 3. Confirm the 2.0 migration is a no-op

- [ ] 3.1 Confirm the repository has no OpenSpec-generated integration to migrate: `git ls-files | grep -Ei 'opsx|\.agents/|\.opencode/|\.claude/|openspec/AGENTS\.md'` returns no matches. Verify the empty result, and add one sentence to the PR description recording that no `openspec update` run is needed.

## 4. Verify the gate under 2.0.0

- [ ] 4.1 Run the script end to end: `scripts/validate-openspec.sh` (equivalently `npx -y @fission-ai/openspec@2.0.0 validate --all --strict --no-interactive`). Expect every committed spec to pass and only the change under construction to be reported. Verify the exit status is 0 and there are no `--strict` findings against `openspec/specs/**`.
- [ ] 4.2 Confirm no committed spec needed changing for the stricter 1.16+/2.0 rules: `git diff --stat` for the whole change touches only `scripts/validate-openspec.sh`, `AGENTS.md` and `.pre-commit-config.yaml` — no file under `openspec/specs/` and no archived change. If any spec is flagged, fix it in the same group rather than widening the scope.
- [ ] 4.3 Exercise the pre-commit hook itself: `pre-commit run openspec-validate --all-files --hook-stage pre-commit` (or commit any file under `openspec/` and let the hook run). Verify the hook exits 0 using the pinned 2.0.0 CLI.
- [ ] 4.4 Negative check on the floor: run the pinned 2.0.0 CLI with an older Node if one is available and confirm the failure is the package's `EBADENGINE`/engine message rather than an obscure runtime error, so the contributor-facing wording in 2.2/2.3 is the right advice. Skip and note "not reproducible locally" when no older Node is on hand.

## Workflow follow-up

- Archive the change with `openspec archive update-openspec-pin-to-2` after the PR is approved and merged (it has no spec deltas to sync).
- If a future OpenSpec major makes the pin drift again, the script comment and the "Risks / Trade-offs" section of `design.md` describe the intended process; a scheduled `openspec version --check` job is a deliberate non-goal of this change.
