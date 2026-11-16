#
# Pre-script: give [env:native] a host entry point.
#
# PlatformIO's build_src_filter is rooted at $PROJECT_SRC_DIR (default: src/),
# so files outside that directory cannot be included by extending the filter.
# `pio run -e native` still builds and links a program, and the native filter
# deliberately excludes src/main.cpp (the ESP32 entry point), so the link fails
# with "undefined symbol _main" unless a host main exists.
#
# Test builds define their own main() in test/test_*/. Skip this file for them
# so the placeholder and the test entry point do not collide.
#

import os

Import("env")

if env.get("PIOENV") != "native":
    Return()

if "test" in env.get("BUILD_TYPE", ""):
    Return()

project_dir = env.subst("$PROJECT_DIR")
host_dir = os.path.join(project_dir, "scripts", "native_host")

env.BuildSources(
    os.path.join("$BUILD_DIR", "native_host"),
    host_dir,
    "+<*.cpp>",
)
