//
// native_host: placeholder entry point for [env:native].
//
// `pio run -e native` compiles the host-side sources (shows, strip, support)
// but deliberately excludes src/main.cpp, which is the ESP32 entry point and
// does not compile on the host. PlatformIO still links a program, so without
// a main() the link fails with "undefined symbol _main". This file supplies
// one and does nothing: the native environment exists to compile and test the
// portable code, not to run it as an application.
//
// Test builds each provide their own main() in test/test_*/. The SCons glue
// in scripts/native_host_scons.py skips this file for those builds so the two
// entry points cannot collide.
//

int main() {
    return 0;
}
