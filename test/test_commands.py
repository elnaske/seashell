import time

from .utils import run_shell, run_shell_process


def test_echo():
    res = run_shell("""\
        echo hello
        exit
        """)

    assert res.returncode == 0
    assert "hello" in res.stdout


def test_bg_exec():
    p = run_shell_process()

    p.stdin.write("sleep 0 &\n")
    p.stdin.flush()

    time.sleep(0.2)

    _, stderr = p.communicate("exit\n", timeout=2)

    assert "[0] Done" in stderr


def test_empty_line():
    res = run_shell("""\
        
        echo test
        exit
        """)

    assert res.returncode == 0
    assert "test" in res.stdout


def test_empty_line_2():
    res = run_shell("""\
        
        exit
        """)

    assert res.returncode == 0


def test_err_not_a_cmd():
    res = run_shell("""\
        notacommand
        exit
        """)

    assert res.returncode != 0
    assert "Execve error" in res.stderr
