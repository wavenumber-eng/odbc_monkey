// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC
//
// Windows ODBC platform types for odbc-monkey
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <sqltypes.h>

#ifndef SQL_NOUNICODEMAP
#define SQL_NOUNICODEMAP
#define ODBC_MONKEY_UNDEF_SQL_NOUNICODEMAP
#endif

#ifndef RC_INVOKED
#define RC_INVOKED
#define ODBC_MONKEY_UNDEF_RC_INVOKED
#endif

#include <sql.h>
#include <sqlext.h>

#ifdef ODBC_MONKEY_UNDEF_RC_INVOKED
#undef RC_INVOKED
#undef ODBC_MONKEY_UNDEF_RC_INVOKED
#endif

#ifdef ODBC_MONKEY_UNDEF_SQL_NOUNICODEMAP
#undef SQL_NOUNICODEMAP
#undef ODBC_MONKEY_UNDEF_SQL_NOUNICODEMAP
#endif

#ifdef _WIN32
#ifdef ODBC_MONKEY_EXPORTS
#define ODBC_MONKEY_API __declspec(dllexport)
#else
#define ODBC_MONKEY_API __declspec(dllimport)
#endif
#else
#define ODBC_MONKEY_API
#endif

#ifndef SQL_NTSL
#define SQL_NTSL (-3L)
#endif

// Driver-specific constants
#define ODBCMONKEY_DRIVER_NAME L"OdbcMonkey Native Driver"
#define ODBCMONKEY_DRIVER_VER "01.01.0000"
#define ODBCMONKEY_ODBC_VER "03.80"

// Magic numbers for handle validation
#define ENV_MAGIC 0x454E5631  // ENV1
#define DBC_MAGIC 0x44424331  // DBC1
#define STMT_MAGIC 0x53544D31 // STM1
