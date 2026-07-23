import os
import signal
import time

from .utils import run_shell, run_shell_process


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
    assert "[0] Done" not in res.stderr


def test_fg_bare():
    res = run_shell("""\
        sleep 0.2 &
        fg
        exit
        """)

    assert res.returncode == 0
    assert "[0] Done" not in res.stderr


def test_fg_err_no_current_job():
    res = run_shell("""\
        fg
        exit
        """)

    assert res.returncode != 0
    assert "fg" in res.stderr


def test_fg_err_invalid_job_id():
    res = run_shell("""\
        sleep 0.2 &
        fg abc
        exit
        """)

    assert res.returncode != 0
    assert "fg" in res.stderr


def test_fg_err_no_job_ctl():
    res = run_shell("""\
        sleep 0.2 &
        fg 0 &
        exit
        """)

    assert res.returncode != 0
    assert "fg" in res.stderr


def test_bg():
    p = run_shell_process()

    p.stdin.write("sleep 0.2\n")
    p.stdin.flush()

    time.sleep(0.1)

    os.kill(p.pid, signal.SIGTSTP)

    p.stdin.write("bg 0\n")
    p.stdin.flush()

    time.sleep(0.2)

    p.stdin.write("\n")
    p.stdin.flush()

    _, stderr = p.communicate("exit\n", timeout=2)

    assert p.returncode == 0
    assert "[0] Done" in stderr


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
    assert "[0] Done" in stderr

def test_fg_err_invalid_job_id():
    res = run_shell("""\
        bg abc
        exit
        """)

    assert res.returncode != 0
    assert "bg" in res.stderr


def test_bg_err_no_current_job():
    res = run_shell("""\
        bg
        exit
        """)

    assert res.returncode != 0
    assert "bg" in res.stderr


def test_bg_err_already_bg():
    res = run_shell("""\
        sleep 0.2 &
        bg
        exit
        """)

    assert res.returncode != 0
    assert "bg" in res.stderr


def test_bg_err_no_job_ctl():
    res = run_shell("""\
        sleep 0.2 &
        bg 0 &
        exit
        """)

    assert res.returncode != 0
    assert "bg" in res.stderr


def test_jobs_running():
    res = run_shell("""\
        sleep 0.2 &
        jobs
        exit
        """)

    assert res.returncode == 0
    assert "[0] Running" in res.stdout


def test_jobs_stopped():
    p = run_shell_process()

    p.stdin.write("sleep 0.2\n")
    p.stdin.flush()

    time.sleep(0.1)

    os.kill(p.pid, signal.SIGTSTP)

    p.stdin.write("jobs\n")
    p.stdin.flush()

    stdout, _ = p.communicate("exit\n", timeout=2)

    assert p.returncode == 0
    assert "[0] Stopped" in stdout


def test_jobs_resumed():
    p = run_shell_process()

    p.stdin.write("sleep 0.2\n")
    p.stdin.flush()

    time.sleep(0.1)

    os.kill(p.pid, signal.SIGTSTP)

    p.stdin.write("bg 0\n")
    p.stdin.flush()

    p.stdin.write("jobs\n")
    p.stdin.flush()

    stdout, _ = p.communicate("exit\n", timeout=2)

    assert p.returncode == 0
    assert "[0] Running" in stdout


def test_kill():
    res = run_shell("""\
        sleep 10 &
        kill -9 %0
        exit
        """)

    assert res.returncode == 0


def test_kill_err_invalid_signal():
    res = run_shell("""\
        sleep 0.2 &
        kill -abc %0
        exit
        """)

    assert res.returncode != 0
    assert "kill:" in res.stderr


def test_kill_err_invalid_job():
    res = run_shell("""\
        sleep 0.2 &
        kill -9 %abc
        exit
        """)

    assert res.returncode != 0
    assert "kill:" in res.stderr


def test_kill_err_invalid_process():
    res = run_shell("""\
        kill -9 abc
        exit
        """)

    assert res.returncode != 0
    assert "kill:" in res.stderr