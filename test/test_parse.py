import os
import signal
import time

from .utils import run_shell, run_shell_process


def test_parse_err_missing_file():
    res = run_shell("""\
        echo test >
        exit
        """)

    assert res.returncode != 0
    assert "Syntax error" in res.stderr
    assert "test" not in res.stdout


def test_parse_err_missing_file_2():
    res = run_shell("""\
        sort < >
        exit
        """)

    assert res.returncode != 0
    assert "Syntax error" in res.stderr


def test_parse_err_missing_file_3():
    res = run_shell("""\
        sort < |
        exit
        """)

    assert res.returncode != 0
    assert "Syntax error" in res.stderr


def test_parse_err_dangling_pipe():
    res = run_shell("""\
        echo test |
        exit
        """)

    assert res.returncode != 0
    assert "Syntax error" in res.stderr


def test_parse_err_leading_pipe():
    res = run_shell("""\
        | grep test
        exit
        """)

    assert res.returncode != 0
    assert "Syntax error" in res.stderr


def test_parse_err_bg_pipe():
    res = run_shell("""\
        echo abc & | rev
        exit
        """)

    assert res.returncode != 0
    assert "Syntax error" in res.stderr
    assert "abc" not in res.stdout
    assert "cba" not in res.stdout


def test_env_var_expansion():
    res = run_shell("""\
        echo $HOME
        exit
        """)

    assert res.returncode == 0
    assert os.path.expanduser('~') in res.stdout


def test_dollar_no_expansion():
    res = run_shell("""\
        echo test: $
        exit
        """)

    assert res.returncode == 0
    assert 'test: $' in res.stdout


def test_return_status_expansion_0():
    res = run_shell("""\
        echo test
        echo status: $?
        exit
        """)

    assert res.returncode == 0
    assert "status: 0" in res.stdout


def test_return_status_expansion_1():
    res = run_shell("""\
        cd nosuchdir
        echo status: $?
        exit
        """)

    assert res.returncode == 0
    assert "status: 1" in res.stdout


def test_return_status_expansion_three_digit():
    p = run_shell_process()

    p.stdin.write("sleep 10\n")
    p.stdin.flush()

    time.sleep(0.1)

    os.kill(p.pid, signal.SIGINT)

    p.stdin.write("echo status: $?\n")
    p.stdin.flush()

    stdout, _ = p.communicate("exit\n", timeout=2)

    assert p.returncode == 0
    assert "status: 130" in stdout