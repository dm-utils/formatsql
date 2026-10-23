#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include "settings.h"
#include "settings_io.h"
#include "resource.h"

extern HINSTANCE g_module;

FormatSettings g_settings;

// ─── INI persistence ──────────────────────────────────────────────────────────

static std::wstring get_ini_path() {
    wchar_t appdata[MAX_PATH] = {};
    GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    std::wstring dir = std::wstring(appdata) + L"\\Notepad++\\plugins\\config";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\FormatSQL.ini";
}

static const wchar_t* bool_str(bool v) { return v ? L"1" : L"0"; }

static std::wstring to_wide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}
static std::string to_narrow(const wchar_t* w) {
    std::string r;
    for (; *w; ++w) r += (char)(unsigned char)*w;
    return r;
}

static const wchar_t* dialect_to_str(Dialect d) {
    switch (d) {
    case Dialect::Snowflake:  return L"snowflake";
    case Dialect::PostgreSQL: return L"postgresql";
    case Dialect::MSSQL:      return L"mssql";
    case Dialect::MySQL:      return L"mysql";
    case Dialect::SQLite:     return L"sqlite";
    case Dialect::Databricks: return L"databricks";
    default:                  return L"ansi";
    }
}
static Dialect dialect_from_str(const wchar_t* s) {
    if (wcscmp(s, L"snowflake")  == 0) return Dialect::Snowflake;
    if (wcscmp(s, L"postgresql") == 0) return Dialect::PostgreSQL;
    if (wcscmp(s, L"mssql")      == 0) return Dialect::MSSQL;
    if (wcscmp(s, L"mysql")      == 0) return Dialect::MySQL;
    if (wcscmp(s, L"sqlite")     == 0) return Dialect::SQLite;
    if (wcscmp(s, L"databricks") == 0) return Dialect::Databricks;
    return Dialect::ANSI;
}

void save_settings() {
    std::wstring p = get_ini_path();
    const wchar_t* f = p.c_str();
    auto ws = [&](LPCWSTR sec, LPCWSTR key, LPCWSTR val) {
        WritePrivateProfileStringW(sec, key, val, f);
    };
    auto wi = [&](LPCWSTR sec, LPCWSTR key, int val) {
        wchar_t buf[16]; wsprintfW(buf, L"%d", val);
        WritePrivateProfileStringW(sec, key, buf, f);
    };

    ws(L"General", L"Dialect", dialect_to_str(g_settings.dialect));

    ws(L"Casing", L"Keywords",
        g_settings.kw_case == KeywordCase::Preserve ? L"preserve" :
        g_settings.kw_case == KeywordCase::Lower    ? L"lower"    : L"upper");
    ws(L"Casing", L"Functions",
        g_settings.fn_case == KeywordCase::Preserve ? L"preserve" :
        g_settings.fn_case == KeywordCase::Lower    ? L"lower"    : L"upper");
    ws(L"Casing", L"Identifiers",
        g_settings.id_case == IdentCase::Preserve ? L"preserve" :
        g_settings.id_case == IdentCase::Lower    ? L"lower"    : L"upper");

    ws(L"Columns", L"SplitCols",  bool_str(g_settings.split_cols));
    ws(L"Columns", L"CommaPos",   g_settings.comma_pos == CommaPos::Before ? L"before" : L"after");
    ws(L"Columns", L"AliasOp",
        g_settings.alias_op == AliasOp::Add    ? L"add"    :
        g_settings.alias_op == AliasOp::Remove ? L"remove" : L"leave");
    ws(L"Columns", L"AliasCase",  g_settings.alias_case == AliasCase::Lower ? L"lower" : L"upper");

    ws(L"Alignment", L"Keywords", bool_str(g_settings.align_keywords));
    ws(L"Alignment", L"JoinOn",   bool_str(g_settings.align_join_on));

    ws(L"Structure", L"CaseStmt",
        g_settings.case_stmt == CaseStmt::Expand       ? L"expand"        :
        g_settings.case_stmt == CaseStmt::ExpandJoined ? L"expand_joined" :
        g_settings.case_stmt == CaseStmt::Inline       ? L"inline"        : L"leave");
    ws(L"Structure", L"CteNl",     bool_str(g_settings.cte_nl));
    ws(L"Structure", L"CteIndent", bool_str(g_settings.cte_indent));
    ws(L"Structure", L"SubIndent", bool_str(g_settings.sub_indent));
    ws(L"Structure", L"SemiAdd",   bool_str(g_settings.semi_add));
    ws(L"Structure", L"Semicolons",
        g_settings.semi_pos == SemicolonPos::Preserve ? L"preserve" :
        g_settings.semi_pos == SemicolonPos::SameLine ? L"same"     :
        g_settings.semi_pos == SemicolonPos::OwnLine  ? L"own"      : L"remove");

    ws(L"Spacing", L"Operators",
        g_settings.spc_operators == SpacesOpt::Add    ? L"add"    :
        g_settings.spc_operators == SpacesOpt::Remove ? L"remove" : L"leave");
    ws(L"Spacing", L"Functions",
        g_settings.spc_functions == SpacesOpt::Add    ? L"add"    :
        g_settings.spc_functions == SpacesOpt::Remove ? L"remove" : L"leave");
    ws(L"Spacing", L"BlankBetween", bool_str(g_settings.blank_between));
    ws(L"Spacing", L"BlankLines",
        g_settings.blank_lines == BlankLines::Preserve ? L"preserve" :
        g_settings.blank_lines == BlankLines::Remove   ? L"remove"   : L"merge");
    wi(L"Spacing", L"MaxLength", g_settings.max_length);

    ws(L"Joins", L"StripInnerOuter", bool_str(g_settings.strip_inner_outer));

    ws(L"FQDN", L"Database", to_wide(g_settings.fqdn_db).c_str());
    ws(L"FQDN", L"Schema",   to_wide(g_settings.fqdn_schema).c_str());

    // Format on Save is session-only (off after every restart); drop the key
    // that versions before 1.3.2.17 persisted.
    ws(L"General",   L"FormatOnSave",   nullptr);
    ws(L"Columns",   L"AlignAliases",   bool_str(g_settings.align_aliases));
    ws(L"Structure", L"AlignCaseWhen",  bool_str(g_settings.align_case_when));
    wi(L"Spacing",   L"InListWrap",     g_settings.in_list_wrap);
}

void load_settings() {
    std::wstring p = get_ini_path();
    const wchar_t* f = p.c_str();
    wchar_t buf[MAX_PATH] = {};

    GetPrivateProfileStringW(L"General", L"Dialect", L"ansi", buf, 64, f);
    g_settings.dialect = dialect_from_str(buf);

    GetPrivateProfileStringW(L"Casing", L"Keywords", L"lower", buf, 64, f);
    g_settings.kw_case = wcscmp(buf, L"preserve") == 0 ? KeywordCase::Preserve :
                         wcscmp(buf, L"upper")    == 0 ? KeywordCase::Upper    : KeywordCase::Lower;

    GetPrivateProfileStringW(L"Casing", L"Functions", L"lower", buf, 64, f);
    g_settings.fn_case = wcscmp(buf, L"preserve") == 0 ? KeywordCase::Preserve :
                         wcscmp(buf, L"upper")    == 0 ? KeywordCase::Upper    : KeywordCase::Lower;

    GetPrivateProfileStringW(L"Casing", L"Identifiers", L"lower", buf, 64, f);
    g_settings.id_case = wcscmp(buf, L"preserve") == 0 ? IdentCase::Preserve :
                         wcscmp(buf, L"upper")    == 0 ? IdentCase::Upper    : IdentCase::Lower;

    g_settings.split_cols = GetPrivateProfileIntW(L"Columns", L"SplitCols", 1, f) != 0;

    GetPrivateProfileStringW(L"Columns", L"CommaPos", L"before", buf, 64, f);
    g_settings.comma_pos = wcscmp(buf, L"after") == 0 ? CommaPos::After : CommaPos::Before;

    GetPrivateProfileStringW(L"Columns", L"AliasOp", L"add", buf, 64, f);
    g_settings.alias_op = wcscmp(buf, L"remove") == 0 ? AliasOp::Remove   :
                          wcscmp(buf, L"leave")  == 0 ? AliasOp::LeaveAsIs : AliasOp::Add;

    GetPrivateProfileStringW(L"Columns", L"AliasCase", L"lower", buf, 64, f);
    g_settings.alias_case = wcscmp(buf, L"upper") == 0 ? AliasCase::Upper : AliasCase::Lower;

    g_settings.align_keywords = GetPrivateProfileIntW(L"Alignment", L"Keywords", 1, f) != 0;
    g_settings.align_join_on  = GetPrivateProfileIntW(L"Alignment", L"JoinOn",   1, f) != 0;

    GetPrivateProfileStringW(L"Structure", L"CaseStmt", L"expand", buf, 64, f);
    g_settings.case_stmt = wcscmp(buf, L"expand_joined") == 0 ? CaseStmt::ExpandJoined :
                           wcscmp(buf, L"inline")        == 0 ? CaseStmt::Inline        :
                           wcscmp(buf, L"leave")         == 0 ? CaseStmt::LeaveAsIs     : CaseStmt::Expand;

    g_settings.cte_nl     = GetPrivateProfileIntW(L"Structure", L"CteNl",     1, f) != 0;
    g_settings.cte_indent = GetPrivateProfileIntW(L"Structure", L"CteIndent", 1, f) != 0;
    g_settings.sub_indent = GetPrivateProfileIntW(L"Structure", L"SubIndent", 1, f) != 0;
    g_settings.semi_add   = GetPrivateProfileIntW(L"Structure", L"SemiAdd",   0, f) != 0;

    GetPrivateProfileStringW(L"Structure", L"Semicolons", L"same", buf, 64, f);
    g_settings.semi_pos = wcscmp(buf, L"preserve") == 0 ? SemicolonPos::Preserve :
                          wcscmp(buf, L"own")       == 0 ? SemicolonPos::OwnLine  :
                          wcscmp(buf, L"remove")    == 0 ? SemicolonPos::Remove   : SemicolonPos::SameLine;

    GetPrivateProfileStringW(L"Spacing", L"Operators", L"add", buf, 64, f);
    g_settings.spc_operators = wcscmp(buf, L"remove") == 0 ? SpacesOpt::Remove   :
                               wcscmp(buf, L"leave")  == 0 ? SpacesOpt::LeaveAsIs : SpacesOpt::Add;

    GetPrivateProfileStringW(L"Spacing", L"Functions", L"leave", buf, 64, f);
    g_settings.spc_functions = wcscmp(buf, L"add")    == 0 ? SpacesOpt::Add    :
                               wcscmp(buf, L"remove") == 0 ? SpacesOpt::Remove : SpacesOpt::LeaveAsIs;

    g_settings.blank_between = GetPrivateProfileIntW(L"Spacing", L"BlankBetween", 0, f) != 0;

    GetPrivateProfileStringW(L"Spacing", L"BlankLines", L"merge", buf, 64, f);
    g_settings.blank_lines = wcscmp(buf, L"preserve") == 0 ? BlankLines::Preserve :
                             wcscmp(buf, L"remove")   == 0 ? BlankLines::Remove   : BlankLines::Merge;

    g_settings.max_length = (int)GetPrivateProfileIntW(L"Spacing", L"MaxLength", 0, f);

    g_settings.strip_inner_outer = GetPrivateProfileIntW(L"Joins", L"StripInnerOuter", 1, f) != 0;

    g_settings.format_on_save  = false;  // session-only, never read back from the .ini
    g_settings.align_aliases   = GetPrivateProfileIntW(L"Columns",   L"AlignAliases",  0, f) != 0;
    g_settings.align_case_when = GetPrivateProfileIntW(L"Structure", L"AlignCaseWhen", 0, f) != 0;
    g_settings.in_list_wrap    = (int)GetPrivateProfileIntW(L"Spacing", L"InListWrap", 0, f);

    GetPrivateProfileStringW(L"FQDN", L"Database", L"", buf, MAX_PATH, f);
    g_settings.fqdn_db = to_narrow(buf);
    GetPrivateProfileStringW(L"FQDN", L"Schema", L"", buf, MAX_PATH, f);
    g_settings.fqdn_schema = to_narrow(buf);
}

// ─── JSON serialization ───────────────────────────────────────────────────────

static std::string jstr(const char* key, const char* val) {
    return std::string("  \"") + key + "\": \"" + val + "\"";
}
static std::string jbool(const char* key, bool val) {
    return std::string("  \"") + key + "\": " + (val ? "true" : "false");
}
static std::string jint(const char* key, int val) {
    char buf[24] = {};
    wsprintfA(buf, "%d", val);
    return std::string("  \"") + key + "\": " + buf;
}

static std::string json_get_str(const std::string& j, const char* key) {
    std::string pat = std::string("\"") + key + "\": \"";
    auto p = j.find(pat);
    if (p == std::string::npos) return "";
    p += pat.size();
    auto e = j.find('"', p);
    return e == std::string::npos ? "" : j.substr(p, e - p);
}
static bool json_get_bool(const std::string& j, const char* key, bool def) {
    std::string pat = std::string("\"") + key + "\": ";
    auto p = j.find(pat);
    if (p == std::string::npos) return def;
    p += pat.size();
    if (p + 4 <= j.size() && j.compare(p, 4, "true")  == 0) return true;
    if (p + 5 <= j.size() && j.compare(p, 5, "false") == 0) return false;
    return def;
}
static int json_get_int(const std::string& j, const char* key, int def) {
    std::string pat = std::string("\"") + key + "\": ";
    auto p = j.find(pat);
    if (p == std::string::npos) return def;
    p += pat.size();
    if (p < j.size() && (std::isdigit((unsigned char)j[p]) || j[p] == '-'))
        return std::atoi(j.c_str() + p);
    return def;
}

static const char* dialect_to_json(Dialect d) {
    switch (d) {
    case Dialect::Snowflake:  return "snowflake";
    case Dialect::PostgreSQL: return "postgresql";
    case Dialect::MSSQL:      return "mssql";
    case Dialect::MySQL:      return "mysql";
    case Dialect::SQLite:     return "sqlite";
    case Dialect::Databricks: return "databricks";
    default:                  return "ansi";
    }
}
static Dialect dialect_from_json(const std::string& s) {
    if (s == "snowflake")  return Dialect::Snowflake;
    if (s == "postgresql") return Dialect::PostgreSQL;
    if (s == "mssql")      return Dialect::MSSQL;
    if (s == "mysql")      return Dialect::MySQL;
    if (s == "sqlite")     return Dialect::SQLite;
    if (s == "databricks") return Dialect::Databricks;
    return Dialect::ANSI;
}

static std::string settings_to_json(const FormatSettings& s) {
    std::string j = "{\n";
    std::vector<std::string> lines;
    lines.push_back(jstr("dialect",         dialect_to_json(s.dialect)));
    lines.push_back(jstr("kw_case",         s.kw_case == KeywordCase::Upper    ? "upper"    : s.kw_case == KeywordCase::Preserve ? "preserve" : "lower"));
    lines.push_back(jstr("fn_case",         s.fn_case == KeywordCase::Upper    ? "upper"    : s.fn_case == KeywordCase::Preserve ? "preserve" : "lower"));
    lines.push_back(jstr("id_case",         s.id_case == IdentCase::Upper      ? "upper"    : s.id_case == IdentCase::Preserve   ? "preserve" : "lower"));
    lines.push_back(jbool("split_cols",     s.split_cols));
    lines.push_back(jstr("comma_pos",       s.comma_pos == CommaPos::After ? "after" : "before"));
    lines.push_back(jstr("alias_op",        s.alias_op == AliasOp::Remove ? "remove" : s.alias_op == AliasOp::LeaveAsIs ? "leave" : "add"));
    lines.push_back(jstr("alias_case",      s.alias_case == AliasCase::Upper ? "upper" : "lower"));
    lines.push_back(jbool("align_keywords", s.align_keywords));
    lines.push_back(jbool("align_join_on",  s.align_join_on));
    lines.push_back(jstr("case_stmt",       s.case_stmt == CaseStmt::ExpandJoined ? "expand_joined" : s.case_stmt == CaseStmt::Inline ? "inline" : s.case_stmt == CaseStmt::LeaveAsIs ? "leave" : "expand"));
    lines.push_back(jbool("cte_nl",         s.cte_nl));
    lines.push_back(jbool("cte_indent",     s.cte_indent));
    lines.push_back(jbool("sub_indent",     s.sub_indent));
    lines.push_back(jbool("semi_add",       s.semi_add));
    lines.push_back(jstr("semi_pos",        s.semi_pos == SemicolonPos::Preserve ? "preserve" : s.semi_pos == SemicolonPos::OwnLine ? "own" : s.semi_pos == SemicolonPos::Remove ? "remove" : "same"));
    lines.push_back(jstr("spc_operators",   s.spc_operators == SpacesOpt::Add ? "add" : s.spc_operators == SpacesOpt::Remove ? "remove" : "leave"));
    lines.push_back(jstr("spc_functions",   s.spc_functions == SpacesOpt::Add ? "add" : s.spc_functions == SpacesOpt::Remove ? "remove" : "leave"));
    lines.push_back(jbool("blank_between",  s.blank_between));
    lines.push_back(jstr("blank_lines",     s.blank_lines == BlankLines::Preserve ? "preserve" : s.blank_lines == BlankLines::Remove ? "remove" : "merge"));
    lines.push_back(jint("max_length",      s.max_length));
    lines.push_back(jbool("strip_inner_outer", s.strip_inner_outer));
    lines.push_back(jstr("fqdn_db",         s.fqdn_db.c_str()));
    lines.push_back(jstr("fqdn_schema",     s.fqdn_schema.c_str()));
    lines.push_back(jbool("align_aliases",   s.align_aliases));
    lines.push_back(jbool("align_case_when", s.align_case_when));
    lines.push_back(jint("in_list_wrap",     s.in_list_wrap));
    for (size_t i = 0; i < lines.size(); ++i)
        j += lines[i] + (i + 1 < lines.size() ? ",\n" : "\n");
    j += "}\n";
    return j;
}

static FormatSettings settings_from_json(const std::string& j) {
    FormatSettings s;
    auto gs  = [&](const char* k)              { return json_get_str(j, k); };
    auto gb  = [&](const char* k, bool d)      { return json_get_bool(j, k, d); };
    auto gi  = [&](const char* k, int d)       { return json_get_int(j, k, d); };

    s.dialect = dialect_from_json(gs("dialect"));

    auto kw = gs("kw_case");
    s.kw_case = kw == "upper" ? KeywordCase::Upper : kw == "preserve" ? KeywordCase::Preserve : KeywordCase::Lower;
    auto fn = gs("fn_case");
    s.fn_case = fn == "upper" ? KeywordCase::Upper : fn == "preserve" ? KeywordCase::Preserve : KeywordCase::Lower;
    auto id = gs("id_case");
    s.id_case = id == "upper" ? IdentCase::Upper : id == "preserve" ? IdentCase::Preserve : IdentCase::Lower;

    s.split_cols = gb("split_cols", true);
    s.comma_pos  = gs("comma_pos") == "after" ? CommaPos::After : CommaPos::Before;
    auto ao = gs("alias_op");
    s.alias_op   = ao == "remove" ? AliasOp::Remove : ao == "leave" ? AliasOp::LeaveAsIs : AliasOp::Add;
    s.alias_case = gs("alias_case") == "upper" ? AliasCase::Upper : AliasCase::Lower;

    s.align_keywords = gb("align_keywords", true);
    s.align_join_on  = gb("align_join_on", true);

    auto cs = gs("case_stmt");
    s.case_stmt  = cs == "expand_joined" ? CaseStmt::ExpandJoined :
                   cs == "inline"        ? CaseStmt::Inline        :
                   cs == "leave"         ? CaseStmt::LeaveAsIs     : CaseStmt::Expand;
    s.cte_nl     = gb("cte_nl", true);
    s.cte_indent = gb("cte_indent", true);
    s.sub_indent = gb("sub_indent", true);
    s.semi_add   = gb("semi_add", false);
    auto sp = gs("semi_pos");
    s.semi_pos   = sp == "preserve" ? SemicolonPos::Preserve : sp == "own" ? SemicolonPos::OwnLine : sp == "remove" ? SemicolonPos::Remove : SemicolonPos::SameLine;

    auto sop = gs("spc_operators");
    s.spc_operators = sop == "add" ? SpacesOpt::Add : sop == "remove" ? SpacesOpt::Remove : SpacesOpt::LeaveAsIs;
    auto sfn = gs("spc_functions");
    s.spc_functions = sfn == "add" ? SpacesOpt::Add : sfn == "remove" ? SpacesOpt::Remove : SpacesOpt::LeaveAsIs;
    s.blank_between = gb("blank_between", false);
    auto bl = gs("blank_lines");
    s.blank_lines   = bl == "preserve" ? BlankLines::Preserve : bl == "remove" ? BlankLines::Remove : BlankLines::Merge;
    s.max_length    = gi("max_length", 0);
    s.strip_inner_outer = gb("strip_inner_outer", true);

    s.fqdn_db      = gs("fqdn_db");
    s.fqdn_schema  = gs("fqdn_schema");

    s.align_aliases   = gb("align_aliases",   false);
    s.align_case_when = gb("align_case_when", false);
    s.in_list_wrap    = gi("in_list_wrap",    0);

    return s;
}

// ─── profile file operations ──────────────────────────────────────────────────

static std::wstring get_profiles_dir() {
    wchar_t appdata[MAX_PATH] = {};
    GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    std::wstring dir = std::wstring(appdata) + L"\\Notepad++\\plugins\\config\\FormatSQL_profiles";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

static bool write_text_file(const std::wstring& path, const std::string& content) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    WriteFile(h, content.c_str(), (DWORD)content.size(), &written, nullptr);
    CloseHandle(h);
    return true;
}

static std::string read_text_file(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return "";
    LARGE_INTEGER sz; sz.QuadPart = 0;
    GetFileSizeEx(h, &sz);
    if (sz.QuadPart == 0 || sz.QuadPart > 512 * 1024) { CloseHandle(h); return ""; }
    std::string buf((size_t)sz.QuadPart, '\0');
    DWORD nread = 0;
    ReadFile(h, &buf[0], (DWORD)sz.QuadPart, &nread, nullptr);
    CloseHandle(h);
    buf.resize(nread);
    return buf;
}

static std::vector<std::wstring> list_profiles() {
    std::wstring dir = get_profiles_dir();
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW fd = {};
    HANDLE h = FindFirstFileW((dir + L"\\*.json").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return names;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring name = fd.cFileName;
        if (name.size() > 5) names.push_back(name.substr(0, name.size() - 5));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(names.begin(), names.end());
    return names;
}

// ─── tab pages ────────────────────────────────────────────────────────────────

struct TabPage { UINT id; LPCWSTR label; DLGPROC proc; };

// Forward declarations
static void settings_to_ui();
static void ui_to_settings();
static void refresh_profile_list(HWND hPage);

// ── Dialect page ──────────────────────────────────────────────────────────────

static const wchar_t* DIALECT_DESCS[] = {
    L"Standard SQL — generic and cross-platform. Good starting point for any database.",
    L"Snowflake — uppercase convention, QUALIFY, FLATTEN, LATERAL, semi-structured (:field) syntax.",
    L"PostgreSQL — lowercase convention, ::cast, RETURNING, ON CONFLICT, $n parameters.",
    L"MS SQL (T-SQL) — uppercase convention, TOP, NOLOCK hints, [bracket] identifiers.",
    L"MySQL / MariaDB — uppercase convention, LIMIT, backtick identifiers, MySQL functions.",
    L"SQLite — lowercase convention, lightweight single-file database, no stored procedures.",
    L"Databricks (Spark SQL) — uppercase convention, Delta Lake, OPTIMIZE, VACUUM, MERGE INTO.",
};

static Dialect dialect_from_page(HWND hPage) {
    if (IsDlgButtonChecked(hPage, IDC_DIALECT_SNOWFLAKE)  == BST_CHECKED) return Dialect::Snowflake;
    if (IsDlgButtonChecked(hPage, IDC_DIALECT_PG)         == BST_CHECKED) return Dialect::PostgreSQL;
    if (IsDlgButtonChecked(hPage, IDC_DIALECT_MSSQL)      == BST_CHECKED) return Dialect::MSSQL;
    if (IsDlgButtonChecked(hPage, IDC_DIALECT_MYSQL)      == BST_CHECKED) return Dialect::MySQL;
    if (IsDlgButtonChecked(hPage, IDC_DIALECT_SQLITE)     == BST_CHECKED) return Dialect::SQLite;
    if (IsDlgButtonChecked(hPage, IDC_DIALECT_DATABRICKS) == BST_CHECKED) return Dialect::Databricks;
    return Dialect::ANSI;
}

static void update_dialect_desc(HWND hPage) {
    int idx = (int)dialect_from_page(hPage);
    SetDlgItemTextW(hPage, IDC_DIALECT_DESC, DIALECT_DESCS[idx]);
}

static void apply_dialect_preset(Dialect d) {
    g_settings.dialect = d;
    switch (d) {
    case Dialect::ANSI:
        g_settings.kw_case = KeywordCase::Upper;
        g_settings.fn_case = KeywordCase::Upper;
        g_settings.id_case = IdentCase::Lower;
        break;
    case Dialect::Snowflake:
        g_settings.kw_case = KeywordCase::Upper;
        g_settings.fn_case = KeywordCase::Upper;
        g_settings.id_case = IdentCase::Preserve;
        break;
    case Dialect::PostgreSQL:
        g_settings.kw_case = KeywordCase::Lower;
        g_settings.fn_case = KeywordCase::Lower;
        g_settings.id_case = IdentCase::Lower;
        break;
    case Dialect::MSSQL:
        g_settings.kw_case = KeywordCase::Upper;
        g_settings.fn_case = KeywordCase::Preserve;
        g_settings.id_case = IdentCase::Preserve;
        break;
    case Dialect::MySQL:
        g_settings.kw_case = KeywordCase::Upper;
        g_settings.fn_case = KeywordCase::Upper;
        g_settings.id_case = IdentCase::Preserve;
        break;
    case Dialect::SQLite:
        g_settings.kw_case = KeywordCase::Lower;
        g_settings.fn_case = KeywordCase::Lower;
        g_settings.id_case = IdentCase::Lower;
        break;
    case Dialect::Databricks:
        g_settings.kw_case = KeywordCase::Upper;
        g_settings.fn_case = KeywordCase::Upper;
        g_settings.id_case = IdentCase::Preserve;
        break;
    }
}

static INT_PTR CALLBACK dialect_page_proc(HWND hPage, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED) {
            switch (LOWORD(wParam)) {
            case IDC_DIALECT_ANSI:
            case IDC_DIALECT_SNOWFLAKE:
            case IDC_DIALECT_PG:
            case IDC_DIALECT_MSSQL:
            case IDC_DIALECT_MYSQL:
            case IDC_DIALECT_SQLITE:
            case IDC_DIALECT_DATABRICKS:
                update_dialect_desc(hPage);
                break;
            case IDC_DIALECT_APPLY:
                ui_to_settings();
                apply_dialect_preset(dialect_from_page(hPage));
                settings_to_ui();
                break;
            }
        }
        break;
    }
    return FALSE;
}

// ── Profiles page ─────────────────────────────────────────────────────────────

static INT_PTR CALLBACK profiles_page_proc(HWND hPage, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        refresh_profile_list(hPage);
        return TRUE;
    case WM_COMMAND: {
        UINT notif = HIWORD(wParam);
        UINT ctrl  = LOWORD(wParam);

        // Double-click on list = Load
        if (ctrl == IDC_PROFILE_LIST && notif == LBN_DBLCLK)
            goto do_load;

        if (notif != BN_CLICKED) break;
        switch (ctrl) {
        case IDC_PROFILE_LOAD: do_load: {
            HWND hList = GetDlgItem(hPage, IDC_PROFILE_LIST);
            int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
            if (sel == LB_ERR) break;
            wchar_t name[512] = {};
            SendMessageW(hList, LB_GETTEXT, sel, reinterpret_cast<LPARAM>(name));
            std::string json = read_text_file(get_profiles_dir() + L"\\" + name + L".json");
            if (!json.empty()) {
                g_settings = settings_from_json(json);
                settings_to_ui();
                std::wstring msg = std::wstring(L"Profile '") + name + L"' loaded.";
                MessageBoxW(hPage, msg.c_str(), L"Datamodder SQL Formatter", MB_OK | MB_ICONINFORMATION);
            }
            break;
        }
        case IDC_PROFILE_SAVE: {
            wchar_t name[512] = {};
            GetDlgItemTextW(hPage, IDC_PROFILE_NAME, name, 512);
            if (!name[0]) break;
            ui_to_settings();
            write_text_file(get_profiles_dir() + L"\\" + name + L".json", settings_to_json(g_settings));
            refresh_profile_list(hPage);
            break;
        }
        case IDC_PROFILE_DELETE: {
            HWND hList = GetDlgItem(hPage, IDC_PROFILE_LIST);
            int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
            if (sel == LB_ERR) break;
            wchar_t name[512] = {};
            SendMessageW(hList, LB_GETTEXT, sel, reinterpret_cast<LPARAM>(name));
            std::wstring msg2 = std::wstring(L"Delete profile \"") + name + L"\"?";
            if (MessageBoxW(GetParent(hPage), msg2.c_str(), L"Datamodder SQL Formatter",
                            MB_YESNO | MB_ICONQUESTION) == IDYES) {
                DeleteFileW((get_profiles_dir() + L"\\" + name + L".json").c_str());
                refresh_profile_list(hPage);
            }
            break;
        }
        case IDC_PROFILE_EXPORT: {
            wchar_t fname[MAX_PATH] = L"settings";
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = GetParent(hPage);
            ofn.lpstrFilter = L"JSON files\0*.json\0All files\0*.*\0";
            ofn.lpstrFile   = fname;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = L"json";
            if (GetSaveFileNameW(&ofn)) {
                ui_to_settings();
                write_text_file(fname, settings_to_json(g_settings));
            }
            break;
        }
        case IDC_PROFILE_IMPORT: {
            wchar_t fname[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = GetParent(hPage);
            ofn.lpstrFilter = L"JSON files\0*.json\0All files\0*.*\0";
            ofn.lpstrFile   = fname;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                std::string json = read_text_file(fname);
                if (!json.empty()) { g_settings = settings_from_json(json); settings_to_ui(); }
            }
            break;
        }
        case IDC_INI_EXPORT: {
            wchar_t fname[MAX_PATH] = L"formatsql-settings";
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = GetParent(hPage);
            ofn.lpstrFilter = L"INI files\0*.ini\0All files\0*.*\0";
            ofn.lpstrFile   = fname;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = L"ini";
            if (GetSaveFileNameW(&ofn)) {
                ui_to_settings();
                write_text_file(fname, settings_to_ini(g_settings));
            }
            break;
        }
        case IDC_INI_IMPORT: {
            wchar_t fname[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = GetParent(hPage);
            ofn.lpstrFilter = L"INI files\0*.ini\0All files\0*.*\0";
            ofn.lpstrFile   = fname;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                std::string ini = read_text_file(fname);
                if (!ini.empty()) { g_settings = settings_from_ini(ini); settings_to_ui(); }
            }
            break;
        }
        }
        break;
    }
    }
    return FALSE;
}

// ── Other pages ───────────────────────────────────────────────────────────────

static INT_PTR CALLBACK page_proc(HWND, UINT msg, WPARAM, LPARAM) {
    return msg == WM_INITDIALOG ? TRUE : FALSE;
}

static void update_align_case_when_state(HWND hPage) {
    bool joined = IsDlgButtonChecked(hPage, IDC_CASE_JOINED) == BST_CHECKED;
    EnableWindow(GetDlgItem(hPage, IDC_ALIGN_CASE_WHEN), joined ? TRUE : FALSE);
    if (!joined) CheckDlgButton(hPage, IDC_ALIGN_CASE_WHEN, BST_UNCHECKED);
}

static INT_PTR CALLBACK structure_page_proc(HWND hPage, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED) {
            UINT ctrl = LOWORD(wParam);
            if (ctrl == IDC_CASE_LEAVE  || ctrl == IDC_CASE_EXPAND ||
                ctrl == IDC_CASE_JOINED || ctrl == IDC_CASE_INLINE)
                update_align_case_when_state(hPage);
        }
        break;
    }
    return FALSE;
}

static void update_alias_case_visibility(HWND hPage) {
    bool show = IsDlgButtonChecked(hPage, IDC_ALIAS_ADD) == BST_CHECKED;
    int sw = show ? SW_SHOW : SW_HIDE;
    ShowWindow(GetDlgItem(hPage, IDC_ALIAS_KW_LOWER), sw);
    ShowWindow(GetDlgItem(hPage, IDC_ALIAS_KW_UPPER), sw);
}

static INT_PTR CALLBACK columns_page_proc(HWND h, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        ShowWindow(GetDlgItem(h, IDC_ALIAS_KW_LOWER), SW_HIDE);
        ShowWindow(GetDlgItem(h, IDC_ALIAS_KW_UPPER), SW_HIDE);
        return TRUE;
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED &&
            (LOWORD(wParam) == IDC_ALIAS_PRESERVE ||
             LOWORD(wParam) == IDC_ALIAS_ADD       ||
             LOWORD(wParam) == IDC_ALIAS_REMOVE_AS)) {
            update_alias_case_visibility(h);
        }
        break;
    }
    return FALSE;
}

// ── Tab page array ────────────────────────────────────────────────────────────
// Order: 0=Dialect 1=Structure 2=Casing 3=Columns 4=Alignment 5=Spacing 6=JOINs 7=FQDN 8=Profiles

static const TabPage PAGES[] = {
    { IDD_TAB_DIALECT,   L"Dialect",   dialect_page_proc  },
    { IDD_TAB_STRUCTURE, L"Structure", structure_page_proc },
    { IDD_TAB_CASING,    L"Casing",    page_proc          },
    { IDD_TAB_COLUMNS,   L"Columns",   columns_page_proc  },
    { IDD_TAB_ALIGNMENT, L"Alignment", page_proc          },
    { IDD_TAB_SPACING,   L"Spacing",   page_proc          },
    { IDD_TAB_JOINS,     L"JOINs",     page_proc          },
    { IDD_TAB_FQDN,      L"FQDN",      page_proc          },
    { IDD_TAB_PROFILES,  L"Profiles",  profiles_page_proc },
};
static const int PAGE_COUNT = 9;
static HWND g_pages[PAGE_COUNT] = {};

#define P_DIALECT g_pages[0]
#define P_STRUCT  g_pages[1]
#define P_CASING  g_pages[2]
#define P_COLS    g_pages[3]
#define P_ALIGN   g_pages[4]
#define P_SPACE   g_pages[5]
#define P_JOIN    g_pages[6]
#define P_FQDN    g_pages[7]
#define P_PROF    g_pages[8]

static void show_page(int idx) {
    for (int i = 0; i < PAGE_COUNT; ++i)
        ShowWindow(g_pages[i], i == idx ? SW_SHOW : SW_HIDE);
}

// ─── control helpers ──────────────────────────────────────────────────────────

static bool radio_set(HWND dlg, int id) { return IsDlgButtonChecked(dlg, id) == BST_CHECKED; }
static bool chk_set  (HWND dlg, int id) { return IsDlgButtonChecked(dlg, id) == BST_CHECKED; }
static int  edit_int (HWND dlg, int id) {
    wchar_t buf[16] = {};
    GetDlgItemTextW(dlg, id, buf, 16);
    return buf[0] ? _wtoi(buf) : 0;
}
static void set_edit(HWND dlg, int id, int v) {
    wchar_t buf[16]; wsprintfW(buf, L"%d", v);
    SetDlgItemTextW(dlg, id, buf);
}

// ─── profile list helper (definition) ────────────────────────────────────────

static void refresh_profile_list(HWND hPage) {
    HWND hList = GetDlgItem(hPage, IDC_PROFILE_LIST);
    SendMessage(hList, LB_RESETCONTENT, 0, 0);
    for (const auto& name : list_profiles())
        SendMessageW(hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
}

// ─── settings ↔ UI ────────────────────────────────────────────────────────────

static void settings_to_ui() {
    // Dialect
    CheckRadioButton(P_DIALECT, IDC_DIALECT_ANSI, IDC_DIALECT_DATABRICKS,
        g_settings.dialect == Dialect::Snowflake   ? IDC_DIALECT_SNOWFLAKE   :
        g_settings.dialect == Dialect::PostgreSQL  ? IDC_DIALECT_PG          :
        g_settings.dialect == Dialect::MSSQL       ? IDC_DIALECT_MSSQL       :
        g_settings.dialect == Dialect::MySQL       ? IDC_DIALECT_MYSQL       :
        g_settings.dialect == Dialect::SQLite      ? IDC_DIALECT_SQLITE      :
        g_settings.dialect == Dialect::Databricks  ? IDC_DIALECT_DATABRICKS  : IDC_DIALECT_ANSI);
    update_dialect_desc(P_DIALECT);

    // Casing
    CheckRadioButton(P_CASING, IDC_KW_PRESERVE, IDC_KW_UPPER,
        g_settings.kw_case == KeywordCase::Preserve ? IDC_KW_PRESERVE :
        g_settings.kw_case == KeywordCase::Lower    ? IDC_KW_LOWER    : IDC_KW_UPPER);
    CheckRadioButton(P_CASING, IDC_FN_PRESERVE, IDC_FN_UPPER,
        g_settings.fn_case == KeywordCase::Preserve ? IDC_FN_PRESERVE :
        g_settings.fn_case == KeywordCase::Lower    ? IDC_FN_LOWER    : IDC_FN_UPPER);
    CheckRadioButton(P_CASING, IDC_ID_PRESERVE, IDC_ID_UPPER,
        g_settings.id_case == IdentCase::Preserve ? IDC_ID_PRESERVE :
        g_settings.id_case == IdentCase::Lower    ? IDC_ID_LOWER    : IDC_ID_UPPER);

    // Columns
    CheckDlgButton(P_COLS, IDC_SPLIT_COLS,    g_settings.split_cols    ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(P_COLS, IDC_ALIGN_ALIASES, g_settings.align_aliases ? BST_CHECKED : BST_UNCHECKED);
    CheckRadioButton(P_COLS, IDC_COMMA_BEFORE, IDC_COMMA_AFTER,
        g_settings.comma_pos == CommaPos::Before ? IDC_COMMA_BEFORE : IDC_COMMA_AFTER);
    CheckRadioButton(P_COLS, IDC_ALIAS_PRESERVE, IDC_ALIAS_ADD,
        g_settings.alias_op == AliasOp::LeaveAsIs ? IDC_ALIAS_PRESERVE  :
        g_settings.alias_op == AliasOp::Add       ? IDC_ALIAS_ADD       : IDC_ALIAS_REMOVE_AS);
    CheckRadioButton(P_COLS, IDC_ALIAS_KW_LOWER, IDC_ALIAS_KW_UPPER,
        g_settings.alias_case == AliasCase::Lower ? IDC_ALIAS_KW_LOWER : IDC_ALIAS_KW_UPPER);
    update_alias_case_visibility(P_COLS);

    // Alignment
    CheckDlgButton(P_ALIGN, IDC_ALIGN_KEYWORDS, g_settings.align_keywords ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(P_ALIGN, IDC_ALIGN_JOIN_ON,  g_settings.align_join_on  ? BST_CHECKED : BST_UNCHECKED);

    // Structure
    CheckRadioButton(P_STRUCT, IDC_CASE_EXPAND, IDC_CASE_LEAVE,
        g_settings.case_stmt == CaseStmt::LeaveAsIs    ? IDC_CASE_LEAVE   :
        g_settings.case_stmt == CaseStmt::ExpandJoined ? IDC_CASE_JOINED  :
        g_settings.case_stmt == CaseStmt::Expand       ? IDC_CASE_EXPAND  : IDC_CASE_INLINE);
    {
        bool joined = (g_settings.case_stmt == CaseStmt::ExpandJoined);
        EnableWindow(GetDlgItem(P_STRUCT, IDC_ALIGN_CASE_WHEN), joined ? TRUE : FALSE);
        CheckDlgButton(P_STRUCT, IDC_ALIGN_CASE_WHEN,
            (joined && g_settings.align_case_when) ? BST_CHECKED : BST_UNCHECKED);
    }
    CheckDlgButton(P_STRUCT, IDC_CTE_NL,     g_settings.cte_nl     ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(P_STRUCT, IDC_CTE_INDENT, g_settings.cte_indent ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(P_STRUCT, IDC_SUB_INDENT, g_settings.sub_indent ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(P_STRUCT, IDC_SEMI_ADD,   g_settings.semi_add   ? BST_CHECKED : BST_UNCHECKED);
    CheckRadioButton(P_STRUCT, IDC_SEMI_PRESERVE, IDC_SEMI_REMOVE,
        g_settings.semi_pos == SemicolonPos::Preserve ? IDC_SEMI_PRESERVE :
        g_settings.semi_pos == SemicolonPos::SameLine ? IDC_SEMI_SAME     :
        g_settings.semi_pos == SemicolonPos::OwnLine  ? IDC_SEMI_OWN      : IDC_SEMI_REMOVE);

    // Spacing
    CheckRadioButton(P_SPACE, IDC_SPC_OPS_ADD, IDC_SPC_OPS_LEAVE,
        g_settings.spc_operators == SpacesOpt::Add    ? IDC_SPC_OPS_ADD    :
        g_settings.spc_operators == SpacesOpt::Remove ? IDC_SPC_OPS_REMOVE : IDC_SPC_OPS_LEAVE);
    CheckRadioButton(P_SPACE, IDC_SPC_FN_ADD, IDC_SPC_FN_LEAVE,
        g_settings.spc_functions == SpacesOpt::Add    ? IDC_SPC_FN_ADD     :
        g_settings.spc_functions == SpacesOpt::Remove ? IDC_SPC_FN_REMOVE  : IDC_SPC_FN_LEAVE);
    CheckDlgButton(P_SPACE, IDC_BLANK_BETWEEN, g_settings.blank_between ? BST_CHECKED : BST_UNCHECKED);
    CheckRadioButton(P_SPACE, IDC_BLANK_PRESERVE, IDC_BLANK_REMOVE,
        g_settings.blank_lines == BlankLines::Preserve ? IDC_BLANK_PRESERVE :
        g_settings.blank_lines == BlankLines::Remove   ? IDC_BLANK_REMOVE   : IDC_BLANK_MERGE);
    set_edit(P_SPACE, IDC_MAX_LENGTH,  g_settings.max_length);
    set_edit(P_SPACE, IDC_IN_LIST_WRAP, g_settings.in_list_wrap);

    // JOINs
    CheckDlgButton(P_JOIN, IDC_STRIP_IO, g_settings.strip_inner_outer ? BST_CHECKED : BST_UNCHECKED);

    // FQDN
    SetDlgItemTextW(P_FQDN, IDC_FQDN_DB,    to_wide(g_settings.fqdn_db).c_str());
    SetDlgItemTextW(P_FQDN, IDC_FQDN_SCHEMA, to_wide(g_settings.fqdn_schema).c_str());
}

static void ui_to_settings() {
    g_settings.dialect = dialect_from_page(P_DIALECT);

    g_settings.kw_case = radio_set(P_CASING, IDC_KW_PRESERVE) ? KeywordCase::Preserve :
                         radio_set(P_CASING, IDC_KW_LOWER)    ? KeywordCase::Lower    : KeywordCase::Upper;
    g_settings.fn_case = radio_set(P_CASING, IDC_FN_PRESERVE) ? KeywordCase::Preserve :
                         radio_set(P_CASING, IDC_FN_LOWER)    ? KeywordCase::Lower    : KeywordCase::Upper;
    g_settings.id_case = radio_set(P_CASING, IDC_ID_PRESERVE) ? IdentCase::Preserve :
                         radio_set(P_CASING, IDC_ID_LOWER)    ? IdentCase::Lower    : IdentCase::Upper;

    g_settings.split_cols    = chk_set(P_COLS, IDC_SPLIT_COLS);
    g_settings.align_aliases = chk_set(P_COLS, IDC_ALIGN_ALIASES);
    g_settings.comma_pos     = radio_set(P_COLS, IDC_COMMA_BEFORE) ? CommaPos::Before : CommaPos::After;
    g_settings.alias_op   = radio_set(P_COLS, IDC_ALIAS_PRESERVE)  ? AliasOp::LeaveAsIs :
                            radio_set(P_COLS, IDC_ALIAS_ADD)        ? AliasOp::Add       : AliasOp::Remove;
    g_settings.alias_case = radio_set(P_COLS, IDC_ALIAS_KW_LOWER) ? AliasCase::Lower : AliasCase::Upper;

    g_settings.align_keywords = chk_set(P_ALIGN, IDC_ALIGN_KEYWORDS);
    g_settings.align_join_on  = chk_set(P_ALIGN, IDC_ALIGN_JOIN_ON);

    g_settings.case_stmt      = radio_set(P_STRUCT, IDC_CASE_EXPAND)  ? CaseStmt::Expand       :
                               radio_set(P_STRUCT, IDC_CASE_JOINED) ? CaseStmt::ExpandJoined :
                               radio_set(P_STRUCT, IDC_CASE_INLINE) ? CaseStmt::Inline        : CaseStmt::LeaveAsIs;
    g_settings.align_case_when = (g_settings.case_stmt == CaseStmt::ExpandJoined) && chk_set(P_STRUCT, IDC_ALIGN_CASE_WHEN);
    g_settings.cte_nl          = chk_set(P_STRUCT, IDC_CTE_NL);
    g_settings.cte_indent = chk_set(P_STRUCT, IDC_CTE_INDENT);
    g_settings.sub_indent = chk_set(P_STRUCT, IDC_SUB_INDENT);
    g_settings.semi_add   = chk_set(P_STRUCT, IDC_SEMI_ADD);
    g_settings.semi_pos   = radio_set(P_STRUCT, IDC_SEMI_PRESERVE) ? SemicolonPos::Preserve :
                            radio_set(P_STRUCT, IDC_SEMI_SAME)     ? SemicolonPos::SameLine :
                            radio_set(P_STRUCT, IDC_SEMI_OWN)      ? SemicolonPos::OwnLine  : SemicolonPos::Remove;

    g_settings.spc_operators = radio_set(P_SPACE, IDC_SPC_OPS_ADD)    ? SpacesOpt::Add    :
                               radio_set(P_SPACE, IDC_SPC_OPS_REMOVE) ? SpacesOpt::Remove : SpacesOpt::LeaveAsIs;
    g_settings.spc_functions = radio_set(P_SPACE, IDC_SPC_FN_ADD)     ? SpacesOpt::Add    :
                               radio_set(P_SPACE, IDC_SPC_FN_REMOVE)  ? SpacesOpt::Remove : SpacesOpt::LeaveAsIs;
    g_settings.blank_between = chk_set(P_SPACE, IDC_BLANK_BETWEEN);
    g_settings.blank_lines   = radio_set(P_SPACE, IDC_BLANK_PRESERVE) ? BlankLines::Preserve :
                               radio_set(P_SPACE, IDC_BLANK_REMOVE)   ? BlankLines::Remove   : BlankLines::Merge;
    g_settings.max_length    = edit_int(P_SPACE, IDC_MAX_LENGTH);
    g_settings.in_list_wrap  = edit_int(P_SPACE, IDC_IN_LIST_WRAP);

    g_settings.strip_inner_outer = chk_set(P_JOIN, IDC_STRIP_IO);

    wchar_t wbuf[MAX_PATH] = {};
    GetDlgItemTextW(P_FQDN, IDC_FQDN_DB, wbuf, MAX_PATH);
    g_settings.fqdn_db = to_narrow(wbuf);
    GetDlgItemTextW(P_FQDN, IDC_FQDN_SCHEMA, wbuf, MAX_PATH);
    g_settings.fqdn_schema = to_narrow(wbuf);
}

// ─── settings dialog ──────────────────────────────────────────────────────────

static INT_PTR CALLBACK settings_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {

    case WM_INITDIALOG: {
        HWND hNav = GetDlgItem(hDlg, IDC_NAV);
        for (int i = 0; i < PAGE_COUNT; ++i)
            SendMessageW(hNav, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(PAGES[i].label));
        SendMessageW(hNav, LB_SETCURSEL, 0, 0);

        RECT navRc;
        GetWindowRect(hNav, &navRc);
        MapWindowPoints(HWND_DESKTOP, hDlg, reinterpret_cast<POINT*>(&navRc), 2);
        int cx = navRc.right + 5;
        int cy = navRc.top;

        for (int i = 0; i < PAGE_COUNT; ++i) {
            g_pages[i] = CreateDialog(g_module, MAKEINTRESOURCE(PAGES[i].id), hDlg, PAGES[i].proc);
            SetWindowPos(g_pages[i], HWND_TOP, cx, cy, 0, 0, SWP_NOSIZE | SWP_HIDEWINDOW);
        }

        settings_to_ui();
        show_page(0);
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_NAV && HIWORD(wParam) == LBN_SELCHANGE) {
            int sel = (int)SendMessageW(GetDlgItem(hDlg, IDC_NAV), LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) show_page(sel);
            return TRUE;
        }
        if (LOWORD(wParam) == IDOK) {
            ui_to_settings();
            // Validate FQDN: database without schema is unusual and probably a mistake.
            if (!g_settings.fqdn_db.empty() && g_settings.fqdn_schema.empty()) {
                MessageBoxW(hDlg,
                    L"If a database name is filled in, a schema name must also be provided.\n\n"
                    L"Use only the Schema field if you want to prefix without a database name.",
                    L"Datamodder SQL Formatter – Invalid setting", MB_OK | MB_ICONWARNING);
                HWND hNav = GetDlgItem(hDlg, IDC_NAV);
                SendMessageW(hNav, LB_SETCURSEL, 7, 0);
                show_page(7);
                return TRUE;
            }
            save_settings();
            for (int i = 0; i < PAGE_COUNT; ++i)
                if (g_pages[i]) { DestroyWindow(g_pages[i]); g_pages[i] = nullptr; }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            for (int i = 0; i < PAGE_COUNT; ++i)
                if (g_pages[i]) { DestroyWindow(g_pages[i]); g_pages[i] = nullptr; }
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        for (int i = 0; i < PAGE_COUNT; ++i)
            if (g_pages[i]) { DestroyWindow(g_pages[i]); g_pages[i] = nullptr; }
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void show_settings_dialog(HWND parent) {
    DialogBox(g_module, MAKEINTRESOURCE(IDD_SETTINGS), parent, settings_proc);
}

// ─── convert quotes dialog ───────────────────────────────────────────────────

static const wchar_t* QUOTE_OPTIONS[] = { L"Single  (')", L"Double  (\")", L"Backtick  (`)" };

static INT_PTR CALLBACK convert_quotes_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        HWND hFrom = GetDlgItem(hDlg, IDC_QUOTE_FROM);
        HWND hTo   = GetDlgItem(hDlg, IDC_QUOTE_TO);
        for (auto s : QUOTE_OPTIONS) {
            SendMessageW(hFrom, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
            SendMessageW(hTo,   CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
        }
        SendMessageW(hFrom, CB_SETCURSEL, 1, 0);
        SendMessageW(hTo,   CB_SETCURSEL, 0, 0);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void show_convert_quotes_dialog(HWND parent) {
    DialogBox(g_module, MAKEINTRESOURCE(IDD_CONVERT_QUOTES), parent, convert_quotes_proc);
}

// ─── about dialog ─────────────────────────────────────────────────────────────

static INT_PTR CALLBACK about_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemTextW(hDlg, IDC_ABOUT_BODY,
            L"A powerful SQL formatter and converter for Notepad++.\n\n"
            L"Features:\n"
            L"- Format and minify SQL\n"
            L"- Multi-dialect support:\n"
            L"      - Snowflake\n"
            L"      - MS SQL\n"
            L"      - PostgreSQL\n"
            L"      - MySQL\n"
            L"      - Databricks\n"
            L"- Convert Lists (paste clipboard content as a quoted SQL list)\n"
            L"- Quote and comment style conversion\n"
            L"- Number format conversion\n\n"
            L"This plugin is distributed under the MIT license.\n\n"
            L"For usage tips and screenshots, see the plugin page. For updates or to report a bug, visit the project repository:");
        return TRUE;
    case WM_NOTIFY: {
        NMHDR* hdr = reinterpret_cast<NMHDR*>(lParam);
        if ((hdr->idFrom == IDC_ABOUT_LINK || hdr->idFrom == IDC_ABOUT_LINK_WEBSITE) &&
            (hdr->code == NM_CLICK || hdr->code == NM_RETURN)) {
            NMLINK* link = reinterpret_cast<NMLINK*>(lParam);
            ShellExecuteW(hDlg, L"open", link->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void show_about_dialog(HWND parent) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LINK_CLASS };
    InitCommonControlsEx(&icc);
    DialogBox(g_module, MAKEINTRESOURCE(IDD_ABOUT), parent, about_proc);
}
