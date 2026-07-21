
import subprocess

def run_shell(cmd, timeout=5):
    return subprocess.run(
        "./seashell",
        input=cmd,
        text=True,
        capture_output=True,
        timeout=timeout,
    )


def run_shell_process():
    return subprocess.Popen(
        "./seashell",
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )