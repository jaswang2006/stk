import os
import subprocess
import time


# ============================================================================
# Configuration
# ============================================================================
ENABLE_PROFILE = False               # Enable CPU profiling
APP_NAME = "main_csv"               # C++ application name
CPUPROFILE_FREQUENCY = 40000         # Profiler sampling rate (Hz)
PROFILER_LIB = '/usr/lib/x86_64-linux-gnu/libprofiler.so.0'

# Profiler report settings
TARGET_NAMESPACE = "AssetProcessor"  # Focus on this namespace and all its callees
PPROF_DEPTH = 50                     # Max nodes to display in call tree
PPROF_PORT = 8080                    # Web GUI port
PPROF_IGNORE = "std::|__gnu_cxx::"   # Filter out standard library noise


def run_cpp_with_sampling_profile(binary_path):
    """Run C++ binary with gperftools sampling profiler at maximum frequency."""

    profile_file = "profile.out"

    # Kill old pprof web server before starting new profile run
    print("Cleaning up old pprof web server...")
    subprocess.run(["pkill", "-f", f"pprof.*{PPROF_PORT}"], check=False, capture_output=True)
    time.sleep(0.3)  # Brief wait to ensure cleanup

    # Clean up old profile
    if os.path.exists(profile_file):
        os.remove(profile_file)

    print("Using gperftools CPU profiler (maximum sampling frequency for high-frequency code)...")
    start_time = time.time()

    # Run with gperftools at maximum sampling frequency
    env = os.environ.copy()
    env['CPUPROFILE'] = profile_file
    env['CPUPROFILE_FREQUENCY'] = str(CPUPROFILE_FREQUENCY)

    # Use LD_PRELOAD to inject profiler
    if os.path.exists(PROFILER_LIB):
        env['LD_PRELOAD'] = PROFILER_LIB

    subprocess.run([binary_path], env=env, check=True)

    elapsed_time = time.time() - start_time
    print(f"\nProfiling complete (Time: {elapsed_time:.2f}s)")

    # Generate report and open web GUI
    if os.path.exists(profile_file):
        print("\n" + "="*80)
        print(f"SAMPLING PROFILE - Function-Level Statistics ({CPUPROFILE_FREQUENCY} Hz)")
        print("="*80 + "\n")

        # Find pprof command
        pprof_cmd = None
        for cmd in ["pprof", "google-pprof", os.path.expanduser("~/go/bin/pprof")]:
            result = subprocess.run(["which", cmd], capture_output=True, check=False)
            if result.returncode == 0:
                pprof_cmd = cmd
                break

        if not pprof_cmd:
            print("pprof not found. Profile data saved to:", profile_file)
            return

        # Show top functions (sorted by total time including callees)
        print("\n" + "="*80)
        print(f"Top Functions by Total Time (top 20)")
        print("="*80 + "\n")

        pprof_top = subprocess.run(
            [pprof_cmd, "--top", "--cum", "--nodecount=20",
             f"--focus={TARGET_NAMESPACE}",
             f"--hide={PPROF_IGNORE}",
             binary_path, profile_file],
            capture_output=True,
            text=True,
            check=False
        )

        if pprof_top.returncode == 0:
            print(pprof_top.stdout)

        print("For detailed call hierarchy and flame graph, check the Web GUI")

        print("\n" + "="*80)
        print("Starting pprof web server in background...")
        print("="*80 + "\n")

        # Launch pprof web server in background (non-blocking, survives script exit)
        subprocess.Popen(
            [pprof_cmd, f"-http=:{PPROF_PORT}", binary_path, profile_file],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )

        # Wait for server to start, then auto-open browser
        time.sleep(1.5)
        subprocess.run(["cmd.exe", "/c", "start", f"http://localhost:{PPROF_PORT}"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        print(f"✓ Web GUI opened at http://localhost:{PPROF_PORT} (server running in background)")
        print(f"  To stop server: pkill -f 'pprof.*{PPROF_PORT}'\n")
    else:
        print("\nNo profile generated. Install: sudo apt-get install libgoogle-perftools-dev")


def build_cpp_project(app_name, enable_profile_mode=False):
    """Build C++ project with optional profile mode."""
    cpp_project_dir = f"./cpp/projects/{app_name}"
    build_dir = os.path.join(cpp_project_dir, "build")

    # Configure CMake only if build directory doesn't exist
    if not os.path.exists(build_dir):
        cmake_args = ["cmake", "-S", ".", "-B", "build"]
        if enable_profile_mode:
            cmake_args.append("-DPROFILE_MODE=ON")

        subprocess.run(
            cmake_args,
            cwd=cpp_project_dir,
            check=True
        )

    # Build the project
    subprocess.run(
        ["cmake", "--build", "build", "--parallel"],
        cwd=cpp_project_dir,
        check=True
    )


def run_cpp_binary(app_name, use_profiler=False):
    """Run C++ binary with optional profiling."""
    binary_path = f"./bin/app_{app_name}"

    if use_profiler:
        run_cpp_with_sampling_profile(binary_path)
    else:
        start_time = time.time()
        subprocess.run([binary_path], check=True)
        elapsed_time = time.time() - start_time
        print(f"\nExecution time: {elapsed_time:.2f}s ({elapsed_time/60:.2f}min)")


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    original_dir = os.getcwd()

    build_cpp_project(APP_NAME, enable_profile_mode=ENABLE_PROFILE)

    cpp_build_dir = f"./cpp/projects/{APP_NAME}/build"
    os.chdir(cpp_build_dir)

    try:
        run_cpp_binary(APP_NAME, use_profiler=ENABLE_PROFILE)
    finally:
        os.chdir(original_dir)


if __name__ == "__main__":
    main()
