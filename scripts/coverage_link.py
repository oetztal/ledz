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
