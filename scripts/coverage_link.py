#
# Pre-script: coverage link flags only.
#
# This has to run *before* the main build script constructs the program node,
# which is why it is registered with the `pre:` prefix. Anything that needs the
# program node to exist -- post-actions, custom targets -- lives in
# coverage_post.py instead; registering those from here silently does nothing.
#
Import("env")
import sys

if env.get("PIOENV") != "native":
    Return()

build_flags = env.get("BUILD_FLAGS", [])
has_coverage = "--coverage" in build_flags or any(
    "--coverage" in f for f in build_flags if isinstance(f, str)
)

if not has_coverage:
    Return()

if sys.platform.startswith("linux"):
    env.Append(LINKFLAGS=["--coverage", "-lgcov"])
else:
    env.Append(LINKFLAGS=["--coverage"])
