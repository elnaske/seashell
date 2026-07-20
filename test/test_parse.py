from .utils import run_shell


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
