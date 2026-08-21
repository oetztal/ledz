#
# Post-script: stale .gcda cleanup and the `coverage` target.
#
# Registered with `post:` because both hooks need the program node that the
# main build script creates. The same registrations from a `pre:` script attach
# to nothing and fail silently -- which is how this project ended up with a
# coverage post-action that had never once run.
#
Import("env")

import os
import sys

if env.get("PIOENV") != "native":
    Return()

build_flags = env.get("BUILD_FLAGS", [])
has_coverage = "--coverage" in build_flags or any(
    "--coverage" in f for f in build_flags if isinstance(f, str)
)

if not has_coverage:
    Return()


def purge_stale_gcda(source, target, env):
    """Drop .gcda files that no longer match their recompiled object.

    A .gcda accumulates counters across runs, and the coverage runtime merges
    into an existing one at exit. Once the source has been edited the arc
    layout no longer matches what the old .gcda holds, and the merge does not
    fail cleanly: it floods stderr with "cannot merge previous GCDA file:
    corrupt arc tag" and, on macOS, segfaults inside GCDAProfiling.c *after*
    every test has reported PASSED -- so the suite ends up ERRORED with no
    visible cause, and the counters for that file are garbage either way.

    This runs after linking, when every recompiled object has just rewritten
    its .gcno. A .gcno newer than its .gcda means the object was rebuilt and
    the data is stale. A .gcno older than its .gcda means the object was left
    alone and the data came from an earlier test binary in the same `pio test`
    run -- that must be kept, or the report would only ever cover the last
    suite to run.
    """
    build_dir = env.subst("$BUILD_DIR")
    if not os.path.isdir(build_dir):
        return

    removed = []
    for dirpath, _, filenames in os.walk(build_dir):
        for name in filenames:
            if not name.endswith(".gcda"):
                continue
            gcda = os.path.join(dirpath, name)
            gcno = gcda[: -len(".gcda")] + ".gcno"
            try:
                if not os.path.exists(gcno) or os.path.getmtime(gcno) > os.path.getmtime(gcda):
                    os.remove(gcda)
                    removed.append(os.path.relpath(gcda, build_dir))
            except OSError:
                pass

    if removed:
        print("coverage: dropped %d stale .gcda file(s): %s"
              % (len(removed), ", ".join(sorted(removed)[:5]) + ("..." if len(removed) > 5 else "")))


env.AddPostAction("$PROGPATH", purge_stale_gcda)

# `pio test` offers no hook that fires after the last test binary has *run*,
# which is the only moment the .gcda set is complete -- each test directory is
# built and run in turn, and the final run happens with no SCons process alive.
# So the report is an explicit target rather than a post-action that pretends.
env.AddCustomTarget(
    name="coverage",
    dependencies=None,
    actions=[[sys.executable,
              os.path.join(env.subst("$PROJECT_DIR"), "scripts", "coverage_report.py")]],
    title="Coverage report",
    description="Build coverage_report/ from the last 'pio test -e native' run",
    always_build=True,
)
