import subprocess
import pytest


@pytest.fixture(scope="session", autouse=True)
def build():
    res = subprocess.run(
        "make clean && make",
        shell=True,
        capture_output=True,
        text=True, 
    )
    if res.returncode != 0:
        pytest.fail(f"Build error: {res.stderr}") # ty:ignore[invalid-argument-type]
