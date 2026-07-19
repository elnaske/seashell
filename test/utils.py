
import subprocess

def run_shell(cmd):
    return subprocess.run(
        "./seashell",
        input=cmd,
        text=True,
        capture_output=True,
    )


def run_shell_process():
    return subprocess.Popen(
        "./seashell",
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )