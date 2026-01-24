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
    
    # Run lcov to capture coverage data
    os.system("lcov --capture --directory .pio/build/native/ --output-file coverage.info")
    
    # Filter out system headers and tests
    os.system("lcov --remove coverage.info '/usr/*' '*/test/*' '*/.pio/*' --output-file coverage_filtered.info")
    
    # Generate HTML report
    os.system("genhtml coverage_filtered.info --output-directory coverage_report")
    
    print("Coverage report generated in coverage_report/index.html")

# Only run if we are in the native environment and it's a test action
if env.get("PIOENV") == "native":
    # Check if --coverage flag is present in build_flags
    build_flags = env.get("BUILD_FLAGS", [])
    if "--coverage" in build_flags or any("--coverage" in f for f in build_flags if isinstance(f, str)):
        env.AddPostAction("test", generate_coverage)
    else:
        print("Note: Coverage is not enabled for 'native' environment. Add '--coverage' to build_flags in platformio.ini to enable it.")
