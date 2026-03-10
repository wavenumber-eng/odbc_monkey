// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC
//
// ODBC types and constants for odbc-monkey driver
// NOTE: We define ODBC types ourselves to avoid conflicts with dllexport
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// ============================================================================
// ODBC Basic Types (from sqltypes.h)
// ============================================================================

typedef signed char SQLSCHAR;
typedef unsigned char SQLCHAR;
typedef short SQLSMALLINT;
typedef unsigned short SQLUSMALLINT;
typedef long SQLINTEGER;
typedef unsigned long SQLUINTEGER;
typedef INT64 SQLBIGINT;
typedef UINT64 SQLUBIGINT;
typedef float SQLREAL;
typedef double SQLDOUBLE;
typedef double SQLFLOAT;
typedef void* SQLPOINTER;
typedef SQLSMALLINT SQLRETURN;
typedef wchar_t SQLWCHAR;

#ifdef _WIN64
typedef INT64 SQLLEN;
typedef UINT64 SQLULEN;
typedef UINT64 SQLSETPOSIROW;
#else
typedef SQLINTEGER SQLLEN;
typedef SQLUINTEGER SQLULEN;
typedef SQLUSMALLINT SQLSETPOSIROW;
#endif

typedef PVOID SQLHANDLE;
typedef SQLHANDLE SQLHENV;
typedef SQLHANDLE SQLHDBC;
typedef SQLHANDLE SQLHSTMT;
typedef SQLHANDLE SQLHDESC;

// ============================================================================
// SQL Return Codes (from sql.h)
// ============================================================================

#define SQL_SUCCESS 0
#define SQL_SUCCESS_WITH_INFO 1
#define SQL_STILL_EXECUTING 2
#define SQL_ERROR (-1)
#define SQL_INVALID_HANDLE (-2)
#define SQL_NEED_DATA 99
#define SQL_NO_DATA 100

// ============================================================================
// Handle Types (from sql.h)
// ============================================================================

#define SQL_HANDLE_ENV 1
#define SQL_HANDLE_DBC 2
#define SQL_HANDLE_STMT 3
#define SQL_HANDLE_DESC 4

#define SQL_NULL_HANDLE 0

// ============================================================================
// SQL Data Types (from sql.h / sqlext.h)
// ============================================================================

#define SQL_UNKNOWN_TYPE 0
#define SQL_CHAR 1
#define SQL_NUMERIC 2
#define SQL_DECIMAL 3
#define SQL_INTEGER 4
#define SQL_SMALLINT 5
#define SQL_FLOAT 6
#define SQL_REAL 7
#define SQL_DOUBLE 8
#define SQL_DATETIME 9
#define SQL_VARCHAR 12
#define SQL_WCHAR (-8)
#define SQL_WVARCHAR (-9)
#define SQL_WLONGVARCHAR (-10)
#define SQL_TYPE_DATE 91
#define SQL_TYPE_TIME 92
#define SQL_TYPE_TIMESTAMP 93
#define SQL_LONGVARCHAR (-1)
#define SQL_BINARY (-2)
#define SQL_VARBINARY (-3)
#define SQL_LONGVARBINARY (-4)
#define SQL_BIGINT (-5)
#define SQL_TINYINT (-6)
#define SQL_BIT (-7)
#define SQL_GUID (-11)

// C Types
#define SQL_C_CHAR SQL_CHAR
#define SQL_C_WCHAR SQL_WCHAR
#define SQL_C_LONG SQL_INTEGER
#define SQL_C_SHORT SQL_SMALLINT
#define SQL_C_FLOAT SQL_REAL
#define SQL_C_DOUBLE SQL_DOUBLE
#define SQL_C_NUMERIC SQL_NUMERIC
#define SQL_C_DEFAULT 99
#define SQL_C_DATE SQL_TYPE_DATE
#define SQL_C_TIME SQL_TYPE_TIME
#define SQL_C_TIMESTAMP SQL_TYPE_TIMESTAMP
#define SQL_C_BINARY SQL_BINARY
#define SQL_C_BIT SQL_BIT
#define SQL_C_SBIGINT (SQL_BIGINT + SQL_SIGNED_OFFSET)
#define SQL_C_UBIGINT (SQL_BIGINT + SQL_UNSIGNED_OFFSET)
#define SQL_C_TINYINT SQL_TINYINT
#define SQL_C_SLONG (SQL_C_LONG + SQL_SIGNED_OFFSET)
#define SQL_C_SSHORT (SQL_C_SHORT + SQL_SIGNED_OFFSET)
#define SQL_C_STINYINT (SQL_TINYINT + SQL_SIGNED_OFFSET)
#define SQL_C_ULONG (SQL_C_LONG + SQL_UNSIGNED_OFFSET)
#define SQL_C_USHORT (SQL_C_SHORT + SQL_UNSIGNED_OFFSET)
#define SQL_C_UTINYINT (SQL_TINYINT + SQL_UNSIGNED_OFFSET)
#define SQL_C_GUID SQL_GUID

#define SQL_SIGNED_OFFSET (-20)
#define SQL_UNSIGNED_OFFSET (-22)

// ============================================================================
// NULL Values (from sql.h)
// ============================================================================

#define SQL_NULL_DATA (-1)
#define SQL_DATA_AT_EXEC (-2)
#define SQL_NTS (-3)
#define SQL_NTSL (-3L)

// ============================================================================
// Environment Attributes (from sqlext.h)
// ============================================================================

#define SQL_ATTR_ODBC_VERSION 200
#define SQL_ATTR_CONNECTION_POOLING 201
#define SQL_ATTR_CP_MATCH 202
#define SQL_ATTR_OUTPUT_NTS 10001

#define SQL_OV_ODBC3 3UL
#define SQL_OV_ODBC3_80 380UL

// ============================================================================
// Connection Attributes (from sqlext.h)
// ============================================================================

#define SQL_ATTR_ACCESS_MODE 101
#define SQL_ATTR_AUTOCOMMIT 102
#define SQL_ATTR_LOGIN_TIMEOUT 103
#define SQL_ATTR_TRACE 104
#define SQL_ATTR_TRACEFILE 105
#define SQL_ATTR_TRANSLATE_LIB 106
#define SQL_ATTR_TRANSLATE_OPTION 107
#define SQL_ATTR_TXN_ISOLATION 108
#define SQL_ATTR_CURRENT_CATALOG 109
#define SQL_ATTR_ODBC_CURSORS 110
#define SQL_ATTR_QUIET_MODE 111
#define SQL_ATTR_PACKET_SIZE 112
#define SQL_ATTR_CONNECTION_TIMEOUT 113
#define SQL_ATTR_DISCONNECT_BEHAVIOR 114
// SQL_ATTR_ASYNC_ENABLE (4) defined in Statement Attributes section below
#define SQL_ATTR_AUTO_IPD 10001
#define SQL_ATTR_METADATA_ID 10014

#define SQL_MODE_READ_WRITE 0UL
#define SQL_MODE_READ_ONLY 1UL
#define SQL_AUTOCOMMIT_OFF 0UL
#define SQL_AUTOCOMMIT_ON 1UL
#define SQL_AUTOCOMMIT_DEFAULT SQL_AUTOCOMMIT_ON

// ============================================================================
// Statement Attributes (from sqlext.h)
// ============================================================================

#define SQL_ATTR_QUERY_TIMEOUT 0
#define SQL_ATTR_MAX_ROWS 1
#define SQL_ATTR_NOSCAN 2
#define SQL_ATTR_MAX_LENGTH 3
#define SQL_ATTR_ASYNC_ENABLE 4
#define SQL_ATTR_ROW_BIND_TYPE 5
#define SQL_ATTR_CURSOR_TYPE 6
#define SQL_ATTR_CONCURRENCY 7
#define SQL_ATTR_KEYSET_SIZE 8
#define SQL_ATTR_SIMULATE_CURSOR 10
#define SQL_ATTR_RETRIEVE_DATA 11
#define SQL_ATTR_USE_BOOKMARKS 12
#define SQL_ATTR_ROW_NUMBER 14
#define SQL_ATTR_ENABLE_AUTO_IPD 15
#define SQL_ATTR_FETCH_BOOKMARK_PTR 16
#define SQL_ATTR_PARAM_BIND_OFFSET_PTR 17
#define SQL_ATTR_PARAM_BIND_TYPE 18
#define SQL_ATTR_PARAM_OPERATION_PTR 19
#define SQL_ATTR_PARAM_STATUS_PTR 20
#define SQL_ATTR_PARAMS_PROCESSED_PTR 21
#define SQL_ATTR_PARAMSET_SIZE 22
#define SQL_ATTR_ROW_BIND_OFFSET_PTR 23
#define SQL_ATTR_ROW_OPERATION_PTR 24
#define SQL_ATTR_ROW_STATUS_PTR 25
#define SQL_ATTR_ROWS_FETCHED_PTR 26
#define SQL_ATTR_ROW_ARRAY_SIZE 27
#define SQL_ATTR_APP_ROW_DESC 10010
#define SQL_ATTR_APP_PARAM_DESC 10011
#define SQL_ATTR_IMP_ROW_DESC 10012
#define SQL_ATTR_IMP_PARAM_DESC 10013
#define SQL_ATTR_CURSOR_SCROLLABLE (-1)
#define SQL_ATTR_CURSOR_SENSITIVITY (-2)

// Cursor types
#define SQL_CURSOR_FORWARD_ONLY 0UL
#define SQL_CURSOR_KEYSET_DRIVEN 1UL
#define SQL_CURSOR_DYNAMIC 2UL
#define SQL_CURSOR_STATIC 3UL

// Concurrency
#define SQL_CONCUR_READ_ONLY 1
#define SQL_CONCUR_LOCK 2
#define SQL_CONCUR_ROWVER 3
#define SQL_CONCUR_VALUES 4

// Free Statement options
#define SQL_CLOSE 0
#define SQL_DROP 1
#define SQL_UNBIND 2
#define SQL_RESET_PARAMS 3

// ============================================================================
// Column Attributes (from sqlext.h)
// ============================================================================

#define SQL_DESC_COUNT 1001
#define SQL_DESC_TYPE 1002
#define SQL_DESC_LENGTH 1003
#define SQL_DESC_OCTET_LENGTH_PTR 1004
#define SQL_DESC_PRECISION 1005
#define SQL_DESC_SCALE 1006
#define SQL_DESC_DATETIME_INTERVAL_CODE 1007
#define SQL_DESC_NULLABLE 1008
#define SQL_DESC_INDICATOR_PTR 1009
#define SQL_DESC_DATA_PTR 1010
#define SQL_DESC_NAME 1011
#define SQL_DESC_UNNAMED 1012
#define SQL_DESC_OCTET_LENGTH 1013
#define SQL_DESC_ALLOC_TYPE 1099

#define SQL_COLUMN_COUNT 0
#define SQL_COLUMN_NAME 1
#define SQL_COLUMN_TYPE 2
#define SQL_COLUMN_LENGTH 3
#define SQL_COLUMN_PRECISION 4
#define SQL_COLUMN_SCALE 5
#define SQL_COLUMN_DISPLAY_SIZE 6
#define SQL_COLUMN_NULLABLE 7
#define SQL_COLUMN_UNSIGNED 8
#define SQL_COLUMN_MONEY 9
#define SQL_COLUMN_UPDATABLE 10
#define SQL_COLUMN_AUTO_INCREMENT 11
#define SQL_COLUMN_CASE_SENSITIVE 12
#define SQL_COLUMN_SEARCHABLE 13
#define SQL_COLUMN_TYPE_NAME 14
#define SQL_COLUMN_TABLE_NAME 15
#define SQL_COLUMN_OWNER_NAME 16
#define SQL_COLUMN_QUALIFIER_NAME 17
#define SQL_COLUMN_LABEL 18

// Additional ODBC 3.x descriptor fields
#define SQL_DESC_CATALOG_NAME 1024
#define SQL_DESC_SCHEMA_NAME 1025
#define SQL_DESC_TABLE_NAME 1026
#define SQL_DESC_TYPE_NAME 1030
#define SQL_DESC_DISPLAY_SIZE 1032
#define SQL_DESC_UNSIGNED 1033
#define SQL_DESC_FIXED_PREC_SCALE 1034
#define SQL_DESC_UPDATABLE 1035
#define SQL_DESC_AUTO_UNIQUE_VALUE 1036
#define SQL_DESC_CASE_SENSITIVE 1037
#define SQL_DESC_SEARCHABLE 1038

// Searchability values
#define SQL_PRED_NONE 0
#define SQL_PRED_CHAR 1
#define SQL_PRED_BASIC 2
#define SQL_PRED_SEARCHABLE 3

// Named/Unnamed
#define SQL_NAMED 0
#define SQL_UNNAMED 1

// Nullable
#define SQL_NO_NULLS 0
#define SQL_NULLABLE 1
#define SQL_NULLABLE_UNKNOWN 2

// ============================================================================
// SQLGetInfo Values (from sqlext.h)
// ============================================================================

#define SQL_MAX_DRIVER_CONNECTIONS 0
#define SQL_MAXIMUM_DRIVER_CONNECTIONS SQL_MAX_DRIVER_CONNECTIONS
#define SQL_MAX_CONCURRENT_ACTIVITIES 1
#define SQL_MAXIMUM_CONCURRENT_ACTIVITIES SQL_MAX_CONCURRENT_ACTIVITIES
#define SQL_DATA_SOURCE_NAME 2
#define SQL_DRIVER_NAME 6
#define SQL_DRIVER_VER 7
#define SQL_FETCH_DIRECTION 8
#define SQL_ODBC_API_CONFORMANCE 9
#define SQL_ODBC_VER 10
#define SQL_ROW_UPDATES 11
#define SQL_ODBC_SAG_CLI_CONFORMANCE 15
#define SQL_SERVER_NAME 13
#define SQL_SEARCH_PATTERN_ESCAPE 14
#define SQL_ODBC_SQL_CONFORMANCE 15
#define SQL_DBMS_NAME 17
#define SQL_DBMS_VER 18
#define SQL_ACCESSIBLE_TABLES 19
#define SQL_ACCESSIBLE_PROCEDURES 20
#define SQL_PROCEDURES 21
#define SQL_CONCAT_NULL_BEHAVIOR 22
#define SQL_CURSOR_COMMIT_BEHAVIOR 23
#define SQL_CURSOR_ROLLBACK_BEHAVIOR 24
#define SQL_DATA_SOURCE_READ_ONLY 25
#define SQL_DEFAULT_TXN_ISOLATION 26
#define SQL_EXPRESSIONS_IN_ORDERBY 27
#define SQL_IDENTIFIER_CASE 28
#define SQL_IDENTIFIER_QUOTE_CHAR 29
#define SQL_MAX_COLUMN_NAME_LEN 30
#define SQL_MAXIMUM_COLUMN_NAME_LENGTH SQL_MAX_COLUMN_NAME_LEN
#define SQL_MAX_CURSOR_NAME_LEN 31
#define SQL_MAXIMUM_CURSOR_NAME_LENGTH SQL_MAX_CURSOR_NAME_LEN
#define SQL_MAX_SCHEMA_NAME_LEN 32
#define SQL_MAXIMUM_SCHEMA_NAME_LENGTH SQL_MAX_SCHEMA_NAME_LEN
#define SQL_MAX_CATALOG_NAME_LEN 34
#define SQL_MAXIMUM_CATALOG_NAME_LENGTH SQL_MAX_CATALOG_NAME_LEN
#define SQL_MAX_TABLE_NAME_LEN 35
#define SQL_SCROLL_CONCURRENCY 43
#define SQL_TXN_CAPABLE 46
#define SQL_USER_NAME 47
#define SQL_TXN_ISOLATION_OPTION 72
#define SQL_GETDATA_EXTENSIONS 81
#define SQL_NULL_COLLATION 85
#define SQL_ALTER_TABLE 86
#define SQL_ORDER_BY_COLUMNS_IN_SELECT 90
#define SQL_SPECIAL_CHARACTERS 94
#define SQL_MAX_COLUMNS_IN_GROUP_BY 97
#define SQL_MAXIMUM_COLUMNS_IN_GROUP_BY SQL_MAX_COLUMNS_IN_GROUP_BY
#define SQL_MAX_COLUMNS_IN_INDEX 98
#define SQL_MAXIMUM_COLUMNS_IN_INDEX SQL_MAX_COLUMNS_IN_INDEX
#define SQL_MAX_COLUMNS_IN_ORDER_BY 99
#define SQL_MAXIMUM_COLUMNS_IN_ORDER_BY SQL_MAX_COLUMNS_IN_ORDER_BY
#define SQL_MAX_COLUMNS_IN_SELECT 100
#define SQL_MAXIMUM_COLUMNS_IN_SELECT SQL_MAX_COLUMNS_IN_SELECT
#define SQL_MAX_COLUMNS_IN_TABLE 101
#define SQL_MAX_INDEX_SIZE 102
#define SQL_MAXIMUM_INDEX_SIZE SQL_MAX_INDEX_SIZE
#define SQL_MAX_ROW_SIZE 104
#define SQL_MAXIMUM_ROW_SIZE SQL_MAX_ROW_SIZE
#define SQL_MAX_STATEMENT_LEN 105
#define SQL_MAXIMUM_STATEMENT_LENGTH SQL_MAX_STATEMENT_LEN
#define SQL_MAX_TABLES_IN_SELECT 106
#define SQL_MAXIMUM_TABLES_IN_SELECT SQL_MAX_TABLES_IN_SELECT
#define SQL_MAX_USER_NAME_LEN 107
#define SQL_MAXIMUM_USER_NAME_LENGTH SQL_MAX_USER_NAME_LEN
#define SQL_XOPEN_CLI_YEAR 10000
#define SQL_CURSOR_SENSITIVITY 10001
#define SQL_DESCRIBE_PARAMETER 10002
#define SQL_CATALOG_NAME 10003
#define SQL_COLLATION_SEQ 10004
#define SQL_MAX_IDENTIFIER_LEN 10005
#define SQL_MAXIMUM_IDENTIFIER_LENGTH SQL_MAX_IDENTIFIER_LEN

// Info values
#define SQL_IC_UPPER 1
#define SQL_IC_LOWER 2
#define SQL_IC_SENSITIVE 3
#define SQL_IC_MIXED 4

#define SQL_TC_NONE 0
#define SQL_TC_DML 1
#define SQL_TC_ALL 2
#define SQL_TC_DDL_COMMIT 3
#define SQL_TC_DDL_IGNORE 4

#define SQL_GD_ANY_COLUMN 1
#define SQL_GD_ANY_ORDER 2
#define SQL_GD_BLOCK 4
#define SQL_GD_BOUND 8

// ============================================================================
// SQLGetFunctions values (from sqlext.h)
// ============================================================================

#define SQL_API_SQLALLOCCONNECT 1
#define SQL_API_SQLALLOCENV 2
#define SQL_API_SQLALLOCHANDLE 1001
#define SQL_API_SQLALLOCSTMT 3
#define SQL_API_SQLBINDCOL 4
#define SQL_API_SQLBINDPARAMETER 72
#define SQL_API_SQLCANCEL 5
#define SQL_API_SQLCLOSECURSOR 1003
#define SQL_API_SQLCOLATTRIBUTE 6
#define SQL_API_SQLCOLUMNS 40
#define SQL_API_SQLCONNECT 7
#define SQL_API_SQLCOPYDESC 1004
#define SQL_API_SQLDATASOURCES 57
#define SQL_API_SQLDESCRIBECOL 8
#define SQL_API_SQLDISCONNECT 9
#define SQL_API_SQLENDTRAN 1005
#define SQL_API_SQLERROR 10
#define SQL_API_SQLEXECDIRECT 11
#define SQL_API_SQLEXECUTE 12
#define SQL_API_SQLFETCH 13
#define SQL_API_SQLFETCHSCROLL 1021
#define SQL_API_SQLFREECONNECT 14
#define SQL_API_SQLFREEENV 15
#define SQL_API_SQLFREEHANDLE 1006
#define SQL_API_SQLFREESTMT 16
#define SQL_API_SQLGETCONNECTATTR 1007
#define SQL_API_SQLGETCONNECTOPTION 42
#define SQL_API_SQLGETCURSORNAME 17
#define SQL_API_SQLGETDATA 43
#define SQL_API_SQLGETDESCFIELD 1008
#define SQL_API_SQLGETDESCREC 1009
#define SQL_API_SQLGETDIAGFIELD 1010
#define SQL_API_SQLGETDIAGREC 1011
#define SQL_API_SQLGETENVATTR 1012
#define SQL_API_SQLGETFUNCTIONS 44
#define SQL_API_SQLGETINFO 45
#define SQL_API_SQLGETSTMTATTR 1014
#define SQL_API_SQLGETSTMTOPTION 46
#define SQL_API_SQLGETTYPEINFO 47
#define SQL_API_SQLNUMRESULTCOLS 18
#define SQL_API_SQLPARAMDATA 48
#define SQL_API_SQLPREPARE 19
#define SQL_API_SQLPRIMARYKEYS 65
#define SQL_API_SQLPUTDATA 49
#define SQL_API_SQLROWCOUNT 20
#define SQL_API_SQLSETCONNECTATTR 1016
#define SQL_API_SQLSETCONNECTOPTION 50
#define SQL_API_SQLSETCURSORNAME 21
#define SQL_API_SQLSETDESCFIELD 1017
#define SQL_API_SQLSETDESCREC 1018
#define SQL_API_SQLSETENVATTR 1019
#define SQL_API_SQLSETPARAM 22
#define SQL_API_SQLSETSTMTATTR 1020
#define SQL_API_SQLSETSTMTOPTION 51
#define SQL_API_SQLSPECIALCOLUMNS 52
#define SQL_API_SQLSTATISTICS 53
#define SQL_API_SQLTABLES 54
#define SQL_API_SQLTRANSACT 23
#define SQL_API_SQLDRIVERCONNECT 41
#define SQL_API_SQLFOREIGNKEYS 60
#define SQL_API_SQLMORERESULTS 61
#define SQL_API_SQLNATIVESQL 62
#define SQL_API_SQLNUMPARAMS 63
#define SQL_API_SQLPROCEDURECOLUMNS 66
#define SQL_API_SQLPROCEDURES 67
#define SQL_API_SQLSETPOS 68
#define SQL_API_SQLTABLEPRIVILEGES 70
#define SQL_API_SQLCOLUMNPRIVILEGES 56
#define SQL_API_SQLDESCRIBEPARAM 58
#define SQL_API_SQLEXTENDEDFETCH 59
#define SQL_API_SQLBULKOPERATIONS 24

// SQL_API_ALL or SQL_API_ODBC3_ALL_FUNCTIONS
#define SQL_API_ODBC3_ALL_FUNCTIONS 999
#define SQL_API_ODBC3_ALL_FUNCTIONS_SIZE 250

// ============================================================================
// Driver Connect options (from sqlext.h)
// ============================================================================

#define SQL_DRIVER_NOPROMPT 0
#define SQL_DRIVER_COMPLETE 1
#define SQL_DRIVER_PROMPT 2
#define SQL_DRIVER_COMPLETE_REQUIRED 3

// ============================================================================
// API Calling convention
// ============================================================================

#define SQL_API __stdcall

// Window handle type
typedef HWND SQLHWND;

// Boolean values
#define SQL_FALSE 0
#define SQL_TRUE 1

// More info types
#define SQL_DRIVER_ODBC_VER 77

// ============================================================================
// Driver-specific constants
// ============================================================================

#define ODBCMONKEY_DRIVER_NAME L"OdbcMonkey Native Driver"
#define ODBCMONKEY_DRIVER_VER "01.00.0000"
#define ODBCMONKEY_ODBC_VER "03.80"

// Magic numbers for handle validation
#define ENV_MAGIC 0x454E5631  // ENV1
#define DBC_MAGIC 0x44424331  // DBC1
#define STMT_MAGIC 0x53544D31 // STM1
