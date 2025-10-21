import os
import glob
import subprocess
import time
from config.cfg_stk import cfg_stk


def kill_python_processes_windows(current_pid):
    """Kill all running Python processes in Windows, except this script."""
    try:
        tasklist = subprocess.check_output(
            "tasklist", shell=True, encoding='cp1252')
        pids = [line.split()[1] for line in tasklist.splitlines(
        ) if 'python' in line.lower() and line.split()[1] != str(current_pid)]
        for pid in pids:
            subprocess.run(["taskkill", "/F", "/PID", pid], check=True)
    except:
        pass


def kill_python_processes_unix(current_pid):
    """Kill all running Python processes in Unix-like systems, except this script."""
    try:
        ps_output = subprocess.check_output("ps aux", shell=True).decode()
        pids = [line.split()[1] for line in ps_output.splitlines(
        ) if 'python' in line and 'grep' not in line and line.split()[1] != str(current_pid)]
        for pid in pids:
            subprocess.run(["kill", "-9", pid], check=False)
    except:
        pass


def remove_old_files():
    """Remove old result files."""
    for ext in ['*.xlsx', '*.html', '*.json']:
        for file in glob.glob(ext):
            os.remove(file)


def run_cpp_with_tracy(binary_path, original_dir):
    """Run C++ binary with Tracy profiling."""
    tracy_exe = os.path.join(original_dir, "cpp/package/tracy/tracy-profiler.exe")
    
    if os.path.exists(tracy_exe):
        try:
            wsl_ip = subprocess.check_output("hostname -I | awk '{print $1}'", shell=True).decode().strip()
        except:
            wsl_ip = "localhost"
        
        print(f"Starting Tracy Profiler (auto-connect to {wsl_ip}:8086)...")
        wsl_path = subprocess.check_output(["wslpath", "-w", tracy_exe]).decode().strip()
        subprocess.Popen(
            ["cmd.exe", "/c", "start", "", wsl_path, "-a", wsl_ip],
            stdout=subprocess.DEVNULL, 
            stderr=subprocess.DEVNULL
        )
        time.sleep(5)
    
    start_time = time.time()
    subprocess.run([binary_path], check=True)
    elapsed_time = time.time() - start_time
    
    print(f"\nProfiling complete (Time: {elapsed_time:.2f}s)")


def build_cpp_project(app_name, enable_profile_mode=False):
    """Build C++ project with optional profile mode."""
    cpp_project_dir = f"./cpp/projects/{app_name}"
    
    if enable_profile_mode:
        subprocess.run(
            ["cmake", "-S", ".", "-B", "build", "-DPROFILE_MODE=ON"],
            cwd=cpp_project_dir,
            check=True
        )
    
    subprocess.run(
        ["cmake", "--build", "build", "--parallel"],
        cwd=cpp_project_dir,
        check=True
    )
    
    return True


def run_cpp_binary(app_name, use_profiler=False, original_dir=None):
    """Run C++ binary with optional profiling."""
    binary_path = f"./bin/app_{app_name}"
    
    if use_profiler:
        run_cpp_with_tracy(binary_path, original_dir)
    else:
        start_time = time.time()
        subprocess.run([binary_path], check=True)
        elapsed_time = time.time() - start_time
        print(f"\nExecution time: {elapsed_time:.2f}s ({elapsed_time/60:.2f}min)")


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    original_dir = os.getcwd()

    current_pid = os.getpid()
    if os.name == 'nt':
        kill_python_processes_windows(current_pid)
    else:
        kill_python_processes_unix(current_pid)

    remove_old_files()

    app_name = "main_csv"
    
    build_cpp_project(app_name, enable_profile_mode=cfg_stk.profile)
    
    cpp_build_dir = f"./cpp/projects/{app_name}/build"
    os.chdir(cpp_build_dir)
    
    try:
        run_cpp_binary(app_name, use_profiler=cfg_stk.profile, original_dir=original_dir)
    finally:
        os.chdir(original_dir)


if __name__ == "__main__":
    main()
