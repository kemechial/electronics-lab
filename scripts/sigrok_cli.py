"""Locate sigrok-cli without touching PATH.

Order: environment variable SIGROK_CLI, then sigrok-cli on PATH, then the
default Windows install location. Usable as a module or from the shell:

    python scripts/sigrok_cli.py            # prints the resolved path
    python scripts/sigrok_cli.py --version  # runs sigrok-cli with the arguments
"""
import os
import shutil
import subprocess
import sys

DEFAULT_PATH = r"C:\Program Files\sigrok\sigrok-cli\sigrok-cli.exe"


def find_sigrok_cli():
    env = os.environ.get("SIGROK_CLI")
    if env:
        if not os.path.isfile(env):
            sys.exit(f"SIGROK_CLI is set but does not exist: {env}")
        return env
    on_path = shutil.which("sigrok-cli")
    if on_path:
        return on_path
    if os.path.isfile(DEFAULT_PATH):
        return DEFAULT_PATH
    sys.exit("sigrok-cli not found: set SIGROK_CLI, put it on PATH, or install to " + DEFAULT_PATH)


def run(args, **kwargs):
    """Run sigrok-cli with a list of arguments; returns subprocess.CompletedProcess."""
    return subprocess.run([find_sigrok_cli()] + list(args), **kwargs)


if __name__ == "__main__":
    if len(sys.argv) == 1:
        print(find_sigrok_cli())
    else:
        sys.exit(run(sys.argv[1:]).returncode)
