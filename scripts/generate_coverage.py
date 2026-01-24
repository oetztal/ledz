import os
import subprocess

Import("env")

def generate_coverage(source, target, env):
    print("Checking for coverage tools...")
    
    # Check if lcov is available
    try:
        subprocess.check_call(["lcov", "--version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("Warning: 'lcov' not found. Coverage report will not be generated.")
        print("To enable coverage, please install 'lcov' (e.g., 'brew install lcov' on macOS).")
        return

    print("Generating coverage report...")
    
    # Debug: list files
    print("Listing .gcno and .gcda files:")
    os.system("find . -name '*.gcno' -o -name '*.gcda'")
    
    # Run lcov to capture coverage data
    ret = os.system("lcov --capture --directory . --output-file coverage.info")
    if ret != 0:
        print(f"Error: lcov capture failed with return code {ret}")
        return

    # Filter out system headers and tests
    ret = os.system("lcov --remove coverage.info '/usr/*' '*/test/*' '*/.pio/*' --output-file coverage_filtered.info")
    if ret != 0:
        print(f"Error: lcov filter failed with return code {ret}")
        return
    
    # Generate HTML report
    ret = os.system("genhtml coverage_filtered.info --output-directory coverage_report")
    if ret != 0:
        print(f"Error: genhtml failed with return code {ret}")
        return
    
    print("Coverage report generated in coverage_report/index.html")

# Only run if we are in the native environment
if env.get("PIOENV") == "native":
    # Check if --coverage flag is present in build_flags
    build_flags = env.get("BUILD_FLAGS", [])
    if "--coverage" in build_flags or any("--coverage" in f for f in build_flags if isinstance(f, str)):
        # We want to run after tests, but 'test' might be handled differently.
        # Let's try to add it as a general post-action if 'test' target is called.
        print("Registering coverage generation post-action...")
        env.AddPostAction("test", generate_coverage)
        # Also register for check just in case CI uses it
        env.AddPostAction("check", generate_coverage)
    else:
        print("Note: Coverage is not enabled for 'native' environment. Add '--coverage' to build_flags in platformio.ini to enable it.")
