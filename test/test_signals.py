import os
import signal
import time
from .utils import run_shell, run_shell_process


def test_sigint():
    p = run_shell_process()

    p.stdin.write("sleep 10\n")
    p.stdin.flush()

    time.sleep(0.1)

    os.kill(p.pid, signal.SIGINT)

    p.stdin.write("echo done\n")
    p.stdin.flush()

    stdout, _ = p.communicate("exit\n", timeout=2)

    assert p.returncode == 0
    assert "done" in stdout


def test_sigint_returncode():
    p = run_shell_process()

    p.stdin.write("sleep 10\n")
    p.stdin.flush()

    time.sleep(0.1)

    os.kill(p.pid, signal.SIGINT)

    stdout, _ = p.communicate("exit\n", timeout=2)

    assert p.returncode != 0


def test_sigtstp():
    p = run_shell_process()

    p.stdin.write("sleep 10\n")
    p.stdin.flush()

    time.sleep(0.1)

    os.kill(p.pid, signal.SIGTSTP)

    p.stdin.write("echo done\n")
    p.stdin.flush()

    stdout, _ = p.communicate("exit\n", timeout=2)

    assert p.returncode == 0
    assert "done" in stdout


def test_sigtstp_returncode():
    p = run_shell_process()

    p.stdin.write("sleep 10\n")
    p.stdin.flush()

    time.sleep(0.1)

    os.kill(p.pid, signal.SIGTSTP)

    stdout, _ = p.communicate("exit\n", timeout=2)

    assert p.returncode != 0


def test_bg_exec():
    p = run_shell_process()

    p.stdin.write("sleep 0 &\n")
    p.stdin.flush()

    time.sleep(0.2)

    _, stderr = p.communicate("exit\n", timeout=2)

    assert "Reaped" in stderr
