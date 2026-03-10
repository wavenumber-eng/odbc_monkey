# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Wavenumber LLC
"""Allow running odbc_monkey as: python -m odbc_monkey"""

from odbc_monkey.dblib_builder import main

raise SystemExit(main())
