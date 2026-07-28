import os
import tempfile
from .utils import run_shell


def test_pipe():
    res = run_shell("""\
        echo hello world | rev
        exit
        """)

    assert res.returncode == 0
    assert "dlrow olleh" in res.stdout


def test_pipe_2():
    res = run_shell("""\
        echo hello world | grep hello -o | rev
        exit
        """)

    assert res.returncode == 0
    assert "olleh" in res.stdout


def test_pipe_builtin():
    res = run_shell("""\
        sleep 0.2 &
        jobs | grep -o Run | rev
        exit
        """)

    assert res.returncode == 0
    assert "nuR" in res.stdout


def test_pipe_builtin_child_proc():
    res = run_shell("""\
        echo test | cd
        pwd
        exit
        """)

    assert res.returncode == 0
    assert f"{os.getcwd()}\n" in res.stdout


def test_err_sequential_pipes():
    res = run_shell("""\
        echo hello | | rev
        exit
        """)

    assert res.returncode != 0


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
            echo abc | rev &> a.txt
            cd nosuchdir &> b.txt
            cat a.txt
            cat b.txt
            exit
            """)

    assert res.returncode == 0
    assert "cba" in res.stdout
    assert "cd: No such file or directory" in res.stdout


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


def test_redirect_before_args():
    with tempfile.TemporaryDirectory() as tmp:
        res = run_shell(f"""\
            cd {tmp}
            echo hello world > a.txt
            grep > b.txt hello -o a.txt
            cat b.txt
            exit
            """)

        assert res.returncode == 0
        
        with open(f"{tmp}/a.txt", "r") as a:
            assert "hello world" in a.read()
        with open(f"{tmp}/b.txt", "r") as b:
            assert "hello" in b.read()



def test_pipe_and_redirect():
    with tempfile.TemporaryDirectory() as tmp:
        res = run_shell(f"""\
            cd {tmp}
            echo hello | rev > a.txt
            cat a.txt
            exit
            """)

    assert res.returncode == 0
    assert "olleh" in res.stdout
