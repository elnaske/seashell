import os
import subprocess


def run_shell(cmd, timeout=5):
    env = {**os.environ, "SEASHELL_HISTORY_DISABLED": "1"}
    return subprocess.run(
        "./seashell",
        input=cmd,
        text=True,
        capture_output=True,
        timeout=timeout,
        env=env,
    )


def run_shell_process():
    env = {**os.environ, "SEASHELL_HISTORY_DISABLED": "1"}
    return subprocess.Popen(
        "./seashell",
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env,
    )