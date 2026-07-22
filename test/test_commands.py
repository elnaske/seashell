from .utils import run_shell


def test_echo():
    res = run_shell("""\
        echo hello
        exit
        """)

    assert res.returncode == 0
    assert "hello" in res.stdout


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
