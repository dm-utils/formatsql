#pragma once

// ─── version (single source of truth — bump here only) ──────────────────────
#define FORMATSQL_VERSION       "1.3.2.16"
#define FORMATSQL_VERSION_W     L"1.3.2.16"
#define FORMATSQL_VERSION_COMMA 1,3,2,16

// ─── dialog IDs ───────────────────────────────────────────────────────────────
#define IDD_SETTINGS        101
#define IDD_TAB_CASING      102
#define IDD_TAB_COLUMNS     103
#define IDD_TAB_ALIGNMENT   104
#define IDD_TAB_STRUCTURE   105
#define IDD_TAB_SPACING     106
#define IDD_TAB_JOINS       107
#define IDD_CONVERT_QUOTES  108
#define IDD_TAB_DIALECT     109
#define IDD_TAB_FQDN        110
#define IDD_TAB_PROFILES    111
#define IDD_ABOUT           112

// ─── main dialog ──────────────────────────────────────────────────────────────
#define IDC_NAV             201

// ─── casing tab (grouped: 301-303 KW, 304-306 FN, 307-309 ID) ────────────────
#define IDC_KW_PRESERVE     301
#define IDC_KW_LOWER        302
#define IDC_KW_UPPER        303
#define IDC_FN_PRESERVE     304
#define IDC_FN_LOWER        305
#define IDC_FN_UPPER        306
#define IDC_ID_PRESERVE     307
#define IDC_ID_LOWER        308
#define IDC_ID_UPPER        309

// ─── columns & aliases tab (groups: 404-407 alias, 409-410 alias case) ────────
#define IDC_SPLIT_COLS      401
#define IDC_COMMA_BEFORE    402
#define IDC_COMMA_AFTER     403
#define IDC_ALIAS_PRESERVE  404
#define IDC_ALIAS_REMOVE_AS 405
#define IDC_ALIAS_ADD       407
#define IDC_ALIAS_KW_LOWER  409
#define IDC_ALIAS_KW_UPPER  410

// ─── alignment tab ────────────────────────────────────────────────────────────
#define IDC_ALIGN_KEYWORDS  501
#define IDC_ALIGN_JOIN_ON   502
#define IDC_ALIGN_ALIASES   411   // in Columns tab
#define IDC_ALIGN_CASE_WHEN 624   // in Structure tab
#define IDC_IN_LIST_WRAP    706   // in Spacing tab

// ─── structure tab (groups: 601-604 CASE, 620-624 semicolons) ─────────────────
#define IDC_CASE_EXPAND     601
#define IDC_CASE_INLINE     602
#define IDC_CASE_LEAVE      603   // "Preserve"
#define IDC_CASE_JOINED     604   // "Expand joined" (WHEN...THEN on one line)
#define IDC_CTE_NL          610
#define IDC_CTE_INDENT      611
#define IDC_SUB_INDENT      612
#define IDC_SEMI_ADD        619
#define IDC_SEMI_PRESERVE   620
#define IDC_SEMI_SAME       621
#define IDC_SEMI_OWN        622
#define IDC_SEMI_REMOVE     623

// ─── spacing tab (groups: 710-712 operators, 720-722 functions, 730-732 blank) ──
#define IDC_SPC_OPS_ADD     710
#define IDC_SPC_OPS_REMOVE  711
#define IDC_SPC_OPS_LEAVE   712   // "Preserve"
#define IDC_SPC_FN_ADD      720
#define IDC_SPC_FN_REMOVE   721
#define IDC_SPC_FN_LEAVE    722   // "Preserve"
#define IDC_BLANK_BETWEEN   703
#define IDC_BLANK_PRESERVE  730
#define IDC_BLANK_MERGE     731
#define IDC_BLANK_REMOVE    732
#define IDC_MAX_LENGTH      705

// ─── joins tab ───────────────────────────────────────────────────────────────
#define IDC_STRIP_IO        801

// ─── dialect tab (radios 1001-1007 must be sequential for CheckRadioButton) ──
#define IDC_DIALECT_ANSI        1001
#define IDC_DIALECT_SNOWFLAKE   1002
#define IDC_DIALECT_PG          1003
#define IDC_DIALECT_MSSQL       1004
#define IDC_DIALECT_MYSQL       1005
#define IDC_DIALECT_SQLITE      1006
#define IDC_DIALECT_DATABRICKS  1007
#define IDC_DIALECT_APPLY       1010
#define IDC_DIALECT_DESC        1011

// ─── fqdn tab ────────────────────────────────────────────────────────────────
#define IDC_FQDN_DB             1201
#define IDC_FQDN_SCHEMA         1202
#define IDC_FQDN_QUALIFY        1203

// ─── profiles tab ────────────────────────────────────────────────────────────
#define IDC_PROFILE_LIST        1101
#define IDC_PROFILE_NAME        1102
#define IDC_PROFILE_SAVE        1103
#define IDC_PROFILE_LOAD        1104
#define IDC_PROFILE_DELETE      1105
#define IDC_PROFILE_EXPORT      1106
#define IDC_PROFILE_IMPORT      1107
#define IDC_INI_EXPORT          1108
#define IDC_INI_IMPORT          1109

// ─── convert quotes dialog ───────────────────────────────────────────────────
#define IDC_QUOTE_FROM      901
#define IDC_QUOTE_TO        902

// ─── about dialog ─────────────────────────────────────────────────────────────
#define IDC_ABOUT_LINK      1301
#define IDC_ABOUT_BODY      1302
#define IDC_ABOUT_LINK_WEBSITE 1303
