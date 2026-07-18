import pytest
import os
import signal
import subprocess
import tempfile
import time

# TODO: tests that should fail (incl. proper error handling)


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


def test_echo():
    res = run_shell("""\
        echo hello
        exit
        """)

    assert res.returncode == 0
    assert "hello" in res.stdout


def test_cd():
    res = run_shell("""\
        cd /
        pwd
        exit
        """)

    assert res.returncode == 0
    assert "/" in res.stdout


def test_cd_bare():
    res = run_shell("""\
        cd /
        pwd
        exit
        """)

    assert res.returncode == 0
    assert "/home" in res.stdout


def test_redirect_stdin_stdout():
    with tempfile.TemporaryDirectory() as tmp:
        with open(f"{tmp}/a.txt", 'w') as f:
            f.write("2\n1\n3")

        res = run_shell(f"""\
            cd {tmp}
            sort < a.txt > b.txt
            cat b.txt
            exit
            """)

    assert res.returncode == 0
    assert "1\n2\n3" in res.stdout


def test_redirect_stdout_append():
    with tempfile.TemporaryDirectory() as tmp:
        res = run_shell(f"""\
            cd {tmp}
            echo a >> a.txt
            echo b >> a.txt
            cat a.txt
            exit
            """)

    assert res.returncode == 0
    assert "a\nb" in res.stdout


def test_redirect_stderr():
    with tempfile.TemporaryDirectory() as tmp:
        res = run_shell(f"""\
            cd {tmp}
            cd nosuchdir 2> a.txt
            cat a.txt
            exit
            """)

    assert res.returncode == 0
    assert "No such file or directory" in res.stdout


def test_redirect_stderr_append():
    with tempfile.TemporaryDirectory() as tmp:
        res = run_shell(f"""\
            cd {tmp}
            echo a > a.txt
            cd nosuchdir 2>> a.txt
            cat a.txt
            exit
            """)

    assert res.returncode == 0
    assert "a\ncd: No such file or directory" in res.stdout


def test_redirect_both():
    with tempfile.TemporaryDirectory() as tmp:
        res = run_shell(f"""\
            cd {tmp}
            echo a &> a.txt
            cd nosuchdir &> b.txt
            cat a.txt
            cat b.txt
            exit
            """)

    assert res.returncode == 0
    assert "a\ncd: No such file or directory" in res.stdout


def test_redirect_both_append():
    with tempfile.TemporaryDirectory() as tmp:
        res = run_shell(f"""\
            cd {tmp}
            echo a > a.txt
            echo a &>> a.txt
            cd nosuchdir &>> a.txt
            cat a.txt
            exit
            """)

    assert res.returncode == 0
    assert "a\na\ncd: No such file or directory" in res.stdout


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


def test_bg_exec():
    p = run_shell_process()

    p.stdin.write("sleep 0 &\n")
    p.stdin.flush()

    time.sleep(0.2)

    _, stderr = p.communicate("exit\n", timeout=2)

    assert "Reaped" in stderr


def test_parse_missing_file():
    res = run_shell("""\
        echo test >
        exit
        """)

    assert res.returncode == 0
    assert "Parse error" in res.stderr
    assert "test" not in res.stdout


def test_parse_missing_file_2():
    res = run_shell("""\
        sort < >
        exit
        """)

    assert res.returncode == 0
    assert "Parse error" in res.stderr
