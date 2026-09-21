#
# Pre-script: build glue for [env:native_show_sim].
#
# PlatformIO's build_src_filter is rooted at $PROJECT_SRC_DIR (default: src/),
# so files outside that directory cannot be included by extending the filter.
# We instead add scripts/show_simulator/*.cpp to the build explicitly via SCons,
# leaving src_dir and the existing [env:native] filter untouched.
#

import os

Import("env")

if env.get("PIOENV") != "native_show_sim":
    Return()

project_dir = env.subst("$PROJECT_DIR")
simulator_dir = os.path.join(project_dir, "scripts", "show_simulator")

# The simulator lives outside src/ but needs the same include layout
# (`#include "show/factory/ShowFactory.h"` resolves to src/show/factory/...).
# Project root is the natural base for that.
env.Append(
    CPPPATH=[
        project_dir,
        os.path.join(project_dir, "src"),
        os.path.join(project_dir, ".pio", "libdeps", "native_show_sim", "ArduinoJson", "src"),
    ]
)

# Collect .cpp files explicitly (BuildSources expects a src_filter string,
# not a file path, and we have a known-bounded file list).
sources = [
    os.path.join(simulator_dir, name)
    for name in sorted(os.listdir(simulator_dir))
    if name.endswith(".cpp")
]

env.BuildSources(
    os.path.join("$BUILD_DIR", "show_simulator"),
    simulator_dir,
    "+<*.cpp>",
)

# Touch the file list so SCons sees the right dependencies even if the
# directory changes between runs.
_ = sources
