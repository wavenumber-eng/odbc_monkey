#!/usr/bin/env python3
"""Repo-local compatibility wrapper for the packaged DbLib builder."""

from __future__ import annotations

import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent
SRC_ROOT = REPO_ROOT / "src" / "py"

if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from odbc_monkey.dblib_builder import *  # noqa: F401,F403
from odbc_monkey.dblib_builder import main


if __name__ == "__main__":
    raise SystemExit(main())
