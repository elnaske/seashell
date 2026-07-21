import pytest
import os
import signal
import subprocess
import tempfile
import time

from .utils import run_shell, run_shell_process


def test_echo():
    res = run_shell("""\
        echo hello
        exit
        """)

    assert res.returncode == 0
    assert "hello" in res.stdout


def test_cd():
    res = run_shell("""\
        cd ~
        pwd
        exit
        """)

    assert res.returncode == 0
    assert os.path.expanduser('~') in res.stdout


def test_cd_bare():
    res = run_shell("""\
        cd
        pwd
        exit
        """)

    assert res.returncode == 0
    assert os.path.expanduser('~') in res.stdout


def test_fg():
    res = run_shell("""\
        sleep 0.2 &
        fg 0
        exit
        """)

    assert res.returncode == 0
    assert "Reaped" not in res.stderr


def test_fg_bare():
    res = run_shell("""\
        sleep 0.2 &
        fg
        exit
        """)

    assert res.returncode == 0
    assert "Reaped" not in res.stderr


def test_bg():
    p = run_shell_process()

    p.stdin.write("sleep 0.2\n")
    p.stdin.flush()

    time.sleep(0.1)

    os.kill(p.pid, signal.SIGTSTP)

    p.stdin.write("bg\n")
    p.stdin.flush()

    time.sleep(0.2)

    _, stderr = p.communicate("exit\n", timeout=2)

    assert p.returncode == 0
    assert "Reaped" in stderr


def test_bg_bare():
    p = run_shell_process()

    p.stdin.write("sleep 0.2\n")
    p.stdin.flush()

    time.sleep(0.1)

    os.kill(p.pid, signal.SIGTSTP)

    p.stdin.write("bg\n")
    p.stdin.flush()

    time.sleep(0.2)

    _, stderr = p.communicate("exit\n", timeout=2)

    assert p.returncode == 0
    assert "Reaped" in stderr


def test_empty_line():
    res = run_shell("""\
        
        echo test
        exit
        """)

    assert res.returncode == 0
    assert "test" in res.stdout


def test_err_not_a_cmd():
    res = run_shell("""\
        notacommand
        exit
        """)

    assert res.returncode != 0
    assert "Execve error" in res.stderr
