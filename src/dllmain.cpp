#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <cctype>
#include "formatter.h"
#include "settings.h"
#include "dialects.h"

// ─── Notepad++ / Scintilla types ───────────────────────────────────────────────

struct NppData {
    HWND _nppHandle;
    HWND _scintillaMainHandle;
    HWND _scintillaSecondHandle;
};

struct ShortcutKey {
    bool  _isCtrl;
    bool  _isAlt;
    bool  _isShift;
    UCHAR _key;
};

typedef void (*PFUNCPLUGINCMD)();

struct FuncItem {
    wchar_t        _menuItemName[64];
    PFUNCPLUGINCMD _pFunc;
    int            _cmdID;
    bool           _init2Check;
    ShortcutKey*   _pShKey;
};

struct SCNotification {
    HWND      hwndFrom;
    uintptr_t idFrom;
    unsigned int code;
};

enum {
    SCI_GETLENGTH         = 2006,
    SCI_GETTEXT           = 2182,
    SCI_SETTEXT           = 2181,
    SCI_BEGINUNDOACTION   = 2078,
    SCI_ENDUNDOACTION     = 2079,
    SCI_GETSELECTIONSTART = 2143,
    SCI_GETSELECTIONEND   = 2145,
    SCI_REPLACESEL        = 2170,
};

enum {
    NPPM_GETCURRENTSCINTILLA = 2028,
    NPPM_DOOPEN              = 2101, // NPPMSG (2024) + 77
    NPPN_READY               = 1001,
    NPPN_FILEBEFORESAVE      = 1007,
    NPPN_FILESAVED           = 1008,
};


// ─── globals ───────────────────────────────────────────────────────────────────

static NppData   g_npp    = {};
HINSTANCE        g_module = nullptr;
static bool      g_menu_built = false;
static DWORD     g_format_save_tick = 0;

static ShortcutKey g_shortcut_format = { true, true, true, 'F' }; // Ctrl+Alt+Shift+F

// Slot layout (42 items, index 0-41):
//  0   Format SQL (Ctrl+Alt+Shift+F)
//  1   Minify SQL
//  2   Format and Copy
//  3   [FOS] On       } -> "Format on Save" submenu (built in try_build_menu)
//  4   [FOS] Off      }
//  5   --- separator
//  6-9  Paste List (lines / comma / comma+sp / space)
//  10  --- separator
//  11-15  Number format
//  16-21  Quotes
//  22-23  Boolean
//  24-25  Change Casing
//  26-30  Comment style
//  31-37  Dialects: "From <dialect>" x7, each converts to g_settings.dialect
//  38  --- separator
//  39  Settings...
//  40  About  (dialog with clickable repo link)
//  41  Help   (opens help.txt via NPPM_DOOPEN)

static FuncItem g_funcs[42] = {};

// ─── helpers ──────────────────────────────────────────────────────────────────

static HWND current_editor() {
    int which = 0;
    SendMessage(g_npp._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return which ? g_npp._scintillaSecondHandle : g_npp._scintillaMainHandle;
}

static std::string get_text() {
    HWND sci = current_editor();
    int len = (int)SendMessage(sci, SCI_GETLENGTH, 0, 0);
    if (len <= 0) return {};
    std::string buf(len + 1, '\0');
    SendMessage(sci, SCI_GETTEXT, (WPARAM)(len + 1), (LPARAM)buf.data());
    buf.resize(len);
    return buf;
}

static void set_text(const std::string& text) {
    HWND sci = current_editor();
    SendMessage(sci, SCI_BEGINUNDOACTION, 0, 0);
    SendMessage(sci, SCI_SETTEXT, 0, (LPARAM)text.c_str());
    SendMessage(sci, SCI_ENDUNDOACTION, 0, 0);
}

static void replace_sel(const std::string& text) {
    HWND sci = current_editor();
    SendMessage(sci, SCI_BEGINUNDOACTION, 0, 0);
    SendMessage(sci, SCI_REPLACESEL, 0, (LPARAM)text.c_str());
    SendMessage(sci, SCI_ENDUNDOACTION, 0, 0);
}

static void apply_transform(std::string (*fn)(const std::string&)) {
    HWND sci = current_editor();
    int ss = (int)SendMessage(sci, SCI_GETSELECTIONSTART, 0, 0);
    int se = (int)SendMessage(sci, SCI_GETSELECTIONEND,   0, 0);
    if (ss != se) {
        std::string buf = get_text();
        if (se > (int)buf.size()) return;
        replace_sel(fn(buf.substr(ss, se - ss)));
    } else {
        std::string buf = get_text();
        if (buf.empty()) return;
        set_text(fn(buf));
    }
}

// ─── Format SQL ───────────────────────────────────────────────────────────────

static void cmd_format_sql() { apply_transform(format_sql); }
static void cmd_minify_sql() { apply_transform(minify_sql); }

static void cmd_format_copy() {
    HWND sci = current_editor();
    int ss = (int)SendMessage(sci, SCI_GETSELECTIONSTART, 0, 0);
    int se = (int)SendMessage(sci, SCI_GETSELECTIONEND,   0, 0);
    std::string src = (ss != se) ? get_text().substr(ss, se - ss) : get_text();
    if (src.empty()) return;
    std::string result = format_sql(src);
    int wlen = MultiByteToWideChar(CP_UTF8, 0, result.c_str(), (int)result.size(), nullptr, 0);
    if (wlen <= 0) return;
    if (!OpenClipboard(g_npp._nppHandle)) return;
    EmptyClipboard();
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(wchar_t));
    if (hg) {
        wchar_t* p = static_cast<wchar_t*>(GlobalLock(hg));
        if (p) {
            MultiByteToWideChar(CP_UTF8, 0, result.c_str(), (int)result.size(), p, wlen);
            p[wlen] = L'\0';
            GlobalUnlock(hg);
            SetClipboardData(CF_UNICODETEXT, hg);
        } else {
            GlobalFree(hg);
        }
    }
    CloseClipboard();
}

// ─── Format on Save ──────────────────────────────────────────────────────────

static void set_format_on_save_checkmark() {
    HMENU hBar = GetMenu(g_npp._nppHandle);
    if (!hBar) return;
    bool on = g_settings.format_on_save;
    UINT id_on  = (UINT)g_funcs[3]._cmdID;
    UINT id_off = (UINT)g_funcs[4]._cmdID;
    CheckMenuItem(hBar, id_on,  MF_BYCOMMAND | (on  ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hBar, id_off, MF_BYCOMMAND | (!on ? MF_CHECKED : MF_UNCHECKED));
}

static void cmd_fos_on()  { g_settings.format_on_save = true;  save_settings(); set_format_on_save_checkmark(); }
static void cmd_fos_off() { g_settings.format_on_save = false; save_settings(); set_format_on_save_checkmark(); }

// ─── Strip comments ───────────────────────────────────────────────────────────

static void cmd_strip_comments() {
    std::string buf = get_text();
    if (buf.empty()) return;

    std::string out;
    bool in_str = false;
    char str_d = 0;
    size_t i = 0;

    while (i <= buf.size()) {
        size_t ls = i;
        while (i < buf.size() && buf[i] != '\n') ++i;
        size_t le = i;
        bool has_nl = (i < buf.size());
        if (has_nl) ++i;

        std::string chunk;
        bool had_content = false;
        bool hit_comment = false;

        for (size_t j = ls; j < le; ++j) {
            char c = buf[j];
            if (in_str) {
                chunk += c; had_content = true;
                if (c == str_d && (j + 1 >= le || buf[j + 1] != str_d)) in_str = false;
                continue;
            }
            if (c == '\'' || c == '"') { in_str = true; str_d = c; chunk += c; had_content = true; continue; }
            if (c == '-' && j + 1 < le && buf[j + 1] == '-') { hit_comment = true; break; }
            if (c == '\r') continue;
            chunk += c;
            if (c != ' ' && c != '\t') had_content = true;
        }

        if (had_content) {
            while (!chunk.empty() && (chunk.back() == ' ' || chunk.back() == '\t')) chunk.pop_back();
            out += chunk;
            if (has_nl) out += '\n';
        } else if (!hit_comment) {
            if (has_nl) out += '\n';
        }

        if (i >= buf.size() && !has_nl) break;
    }

    set_text(out);
}

// ─── Paste List ───────────────────────────────────────────────────────────────

// Splits on \n AND \t so both Excel column pastes (newlines) and row pastes
// (tab-separated) are handled automatically without any extra steps.
static void do_paste_list(const wchar_t* sep, bool newlines) {
    if (!OpenClipboard(g_npp._nppHandle)) return;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    std::wstring wtext;
    if (h) {
        wchar_t* p = static_cast<wchar_t*>(GlobalLock(h));
        if (p) { wtext = p; GlobalUnlock(h); }
    }
    CloseClipboard();
    if (wtext.empty()) return;

    std::vector<std::wstring> items;
    std::wstring cur;
    bool in_q = false; wchar_t q_char = 0;

    for (size_t j = 0; j <= wtext.size(); ++j) {
        wchar_t c = (j < wtext.size()) ? wtext[j] : L'\n';
        if (in_q) {
            if (c == q_char) { in_q = false; continue; }
            cur += c; continue;
        }
        if (c == L'\'' || c == L'"') { in_q = true; q_char = c; continue; }
        if (c == L'\r') continue;
        if (c == L'\n' || c == L'\t' || c == L',' || c == L';') {
            size_t s = cur.find_first_not_of(L" \t");
            size_t e = cur.find_last_not_of(L" \t");
            if (s != std::wstring::npos) items.push_back(cur.substr(s, e - s + 1));
            cur.clear();
        } else {
            cur += c;
        }
    }

    if (items.empty()) return;

    auto is_numeric = [](const std::wstring& s) -> bool {
        if (s.empty()) return false;
        size_t k = 0;
        if (s[k] == L'-' || s[k] == L'+') ++k;
        bool has_digit = false, has_dot = false;
        for (; k < s.size(); ++k) {
            if (s[k] >= L'0' && s[k] <= L'9') { has_digit = true; }
            else if (s[k] == L'.' && !has_dot) { has_dot = true; }
            else return false;
        }
        return has_digit;
    };

    std::wstring result;
    for (size_t k = 0; k < items.size(); ++k) {
        if (k > 0) { result += sep; if (newlines) result += L'\n'; }
        if (is_numeric(items[k])) {
            result += items[k];
        } else {
            result += L'\'';
            for (wchar_t c : items[k]) { if (c == L'\'') result += L'\''; result += c; }
            result += L'\'';
        }
    }

    int len = WideCharToMultiByte(CP_UTF8, 0, result.c_str(), (int)result.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return;
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, result.c_str(), (int)result.size(), utf8.data(), len, nullptr, nullptr);
    replace_sel(utf8);
}

static void cmd_paste_lines()    { do_paste_list(L",",  true);  }
static void cmd_paste_comma()    { do_paste_list(L",",  false); }
static void cmd_paste_comma_sp() { do_paste_list(L", ", false); }
static void cmd_paste_space()    { do_paste_list(L" ",  false); }

// ─── Number format ────────────────────────────────────────────────────────────

static void swap_two_chars(char a, char b) {
    std::string s = get_text(); if (s.empty()) return;
    for (char& c : s) if (c == a) c = '\x01';
    for (char& c : s) if (c == b) c = a;
    for (char& c : s) if (c == '\x01') c = b;
    set_text(s);
}

static void cmd_num_nl_to_en() { swap_two_chars('.', ','); }
static void cmd_num_en_to_nl() { swap_two_chars(',', '.'); }

static std::string fn_fr_to_en(const std::string& sql) {
    std::string tmp;
    for (size_t i = 0; i < sql.size(); ++i) {
        if (sql[i] == ' ' && i > 0 && i+1 < sql.size() &&
            std::isdigit((unsigned char)sql[i-1]) &&
            std::isdigit((unsigned char)sql[i+1])) continue;
        tmp += sql[i];
    }
    std::string out;
    for (size_t i = 0; i < tmp.size(); ++i) {
        if (tmp[i] == ',' && i > 0 && i+1 < tmp.size() &&
            std::isdigit((unsigned char)tmp[i-1]) &&
            std::isdigit((unsigned char)tmp[i+1]))
            out += '.';
        else
            out += tmp[i];
    }
    return out;
}

static std::string fn_en_to_fr(const std::string& sql) {
    std::string tmp;
    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];
        if (c == ',' && i > 0 && i+3 < sql.size() &&
            std::isdigit((unsigned char)sql[i-1]) &&
            std::isdigit((unsigned char)sql[i+1]) &&
            std::isdigit((unsigned char)sql[i+2]) &&
            std::isdigit((unsigned char)sql[i+3]) &&
            (i+4 >= sql.size() || !std::isdigit((unsigned char)sql[i+4])))
            tmp += ' ';
        else
            tmp += c;
    }
    std::string out;
    for (size_t i = 0; i < tmp.size(); ++i) {
        if (tmp[i] == '.' && i > 0 && i+1 < tmp.size() &&
            std::isdigit((unsigned char)tmp[i-1]) &&
            std::isdigit((unsigned char)tmp[i+1]))
            out += ',';
        else
            out += tmp[i];
    }
    return out;
}

static std::string fn_remove_thousands(const std::string& sql) {
    std::string out;
    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];
        if ((c == ',' || c == '.') &&
            i > 0 && i+3 < sql.size() &&
            std::isdigit((unsigned char)sql[i-1]) &&
            std::isdigit((unsigned char)sql[i+1]) &&
            std::isdigit((unsigned char)sql[i+2]) &&
            std::isdigit((unsigned char)sql[i+3]) &&
            (i+4 >= sql.size() || !std::isdigit((unsigned char)sql[i+4])))
            continue;
        out += c;
    }
    return out;
}

static void cmd_fr_to_en()         { apply_transform(fn_fr_to_en); }
static void cmd_en_to_fr()         { apply_transform(fn_en_to_fr); }
static void cmd_remove_thousands()  { apply_transform(fn_remove_thousands); }

// ─── Quotes ───────────────────────────────────────────────────────────────────

static void replace_all_chars(char from, char to) {
    std::string s = get_text(); if (s.empty()) return;
    for (char& c : s) if (c == from) c = to;
    set_text(s);
}

static void cmd_quote_dq_to_sq() { replace_all_chars('"',  '\''); }
static void cmd_quote_sq_to_dq() { replace_all_chars('\'', '"');  }
static void cmd_quote_bt_to_sq() { replace_all_chars('`',  '\''); }
static void cmd_quote_sq_to_bt() { replace_all_chars('\'', '`');  }
static void cmd_quote_dq_to_bt() { replace_all_chars('"',  '`');  }
static void cmd_quote_bt_to_dq() { replace_all_chars('`',  '"');  }

// ─── Boolean ──────────────────────────────────────────────────────────────────

static std::string fn_bool_to_int(const std::string& sql) {
    std::string out;
    bool in_str = false; char str_d = 0;
    size_t i = 0;
    while (i < sql.size()) {
        char c = sql[i];
        if (in_str) {
            out += c;
            if (c == str_d && !(i+1 < sql.size() && sql[i+1] == str_d)) in_str = false;
            ++i; continue;
        }
        if (c == '\'' || c == '"') { in_str = true; str_d = c; out += c; ++i; continue; }
        bool wb = (i == 0 || (!std::isalnum((unsigned char)sql[i-1]) && sql[i-1] != '_'));
        if (wb && std::toupper((unsigned char)c) == 'T' && i+4 <= sql.size() &&
            std::toupper((unsigned char)sql[i+1]) == 'R' &&
            std::toupper((unsigned char)sql[i+2]) == 'U' &&
            std::toupper((unsigned char)sql[i+3]) == 'E' &&
            (i+4 >= sql.size() || (!std::isalnum((unsigned char)sql[i+4]) && sql[i+4] != '_'))) {
            out += '1'; i += 4; continue;
        }
        if (wb && std::toupper((unsigned char)c) == 'F' && i+5 <= sql.size() &&
            std::toupper((unsigned char)sql[i+1]) == 'A' &&
            std::toupper((unsigned char)sql[i+2]) == 'L' &&
            std::toupper((unsigned char)sql[i+3]) == 'S' &&
            std::toupper((unsigned char)sql[i+4]) == 'E' &&
            (i+5 >= sql.size() || (!std::isalnum((unsigned char)sql[i+5]) && sql[i+5] != '_'))) {
            out += '0'; i += 5; continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string fn_int_to_bool(const std::string& sql) {
    std::string out;
    bool in_str = false; char str_d = 0;
    size_t i = 0;
    while (i < sql.size()) {
        char c = sql[i];
        if (in_str) {
            out += c;
            if (c == str_d && !(i+1 < sql.size() && sql[i+1] == str_d)) in_str = false;
            ++i; continue;
        }
        if (c == '\'' || c == '"') { in_str = true; str_d = c; out += c; ++i; continue; }
        if ((c == '1' || c == '0') &&
            (i == 0 || (!std::isalnum((unsigned char)sql[i-1]) && sql[i-1] != '_')) &&
            (i+1 >= sql.size() || (!std::isalnum((unsigned char)sql[i+1]) && sql[i+1] != '_' && sql[i+1] != '.'))) {
            out += (c == '1') ? "TRUE" : "FALSE";
            ++i; continue;
        }
        out += c; ++i;
    }
    return out;
}

static void cmd_bool_to_int() { apply_transform(fn_bool_to_int); }
static void cmd_int_to_bool() { apply_transform(fn_int_to_bool); }

// ─── Change Casing (excl. string literals) ────────────────────────────────────

static std::string fn_lowercase_all(const std::string& buf) {
    std::string out;
    bool in_str = false; char str_d = 0;
    for (size_t i = 0; i < buf.size(); ++i) {
        char c = buf[i];
        if (in_str) { out += c; if (c == str_d && !(i+1 < buf.size() && buf[i+1] == str_d)) in_str = false; continue; }
        if (c == '\'' || c == '"') { in_str = true; str_d = c; out += c; continue; }
        out += (char)std::tolower((unsigned char)c);
    }
    return out;
}

static std::string fn_uppercase_all(const std::string& buf) {
    std::string out;
    bool in_str = false; char str_d = 0;
    for (size_t i = 0; i < buf.size(); ++i) {
        char c = buf[i];
        if (in_str) { out += c; if (c == str_d && !(i+1 < buf.size() && buf[i+1] == str_d)) in_str = false; continue; }
        if (c == '\'' || c == '"') { in_str = true; str_d = c; out += c; continue; }
        out += (char)std::toupper((unsigned char)c);
    }
    return out;
}

static void cmd_lowercase_all() { apply_transform(fn_lowercase_all); }
static void cmd_uppercase_all() { apply_transform(fn_uppercase_all); }

// ─── Comment style ────────────────────────────────────────────────────────────

static std::string fn_dash_to_block(const std::string& sql) {
    std::string out;
    bool in_str = false; char str_d = 0;
    size_t i = 0;
    while (i < sql.size()) {
        char c = sql[i];
        if (in_str) { out += c; if (c == str_d && !(i+1 < sql.size() && sql[i+1] == str_d)) in_str = false; ++i; continue; }
        if (c == '\'' || c == '"') { in_str = true; str_d = c; out += c; ++i; continue; }
        if (c == '-' && i+1 < sql.size() && sql[i+1] == '-') {
            size_t j = i + 2;
            while (j < sql.size() && sql[j] != '\n') ++j;
            std::string comment = sql.substr(i + 2, j - (i + 2));
            while (!comment.empty() && (comment.back() == ' ' || comment.back() == '\r')) comment.pop_back();
            out += "/*"; out += comment; out += " */";
            i = j; continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string fn_block_to_dash(const std::string& sql) {
    std::string out;
    bool in_str = false; char str_d = 0;
    size_t i = 0;
    while (i < sql.size()) {
        char c = sql[i];
        if (in_str) { out += c; if (c == str_d && !(i+1 < sql.size() && sql[i+1] == str_d)) in_str = false; ++i; continue; }
        if (c == '\'' || c == '"') { in_str = true; str_d = c; out += c; ++i; continue; }
        if (c == '/' && i+1 < sql.size() && sql[i+1] == '*') {
            size_t end = sql.find("*/", i + 2);
            if (end == std::string::npos) { out += c; ++i; continue; }
            std::string body = sql.substr(i + 2, end - (i + 2));
            bool multiline = (body.find('\n') != std::string::npos);
            if (!multiline) {
                size_t s = body.find_first_not_of(" \t\r");
                size_t e = body.find_last_not_of(" \t\r");
                out += "--";
                if (s != std::string::npos) { out += ' '; out += body.substr(s, e - s + 1); }
            } else {
                size_t j = 0;
                while (j < body.size()) {
                    size_t ls = j;
                    while (j < body.size() && body[j] != '\n') ++j;
                    std::string line = body.substr(ls, j - ls);
                    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
                    size_t fs = line.find_first_not_of(" \t*");
                    if (fs != std::string::npos) { out += "-- "; out += line.substr(fs); out += '\n'; }
                    if (j < body.size()) ++j;
                }
                if (!out.empty() && out.back() == '\n') out.pop_back();
            }
            i = end + 2; continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string fn_hash_to_dash(const std::string& sql) {
    std::string out;
    bool in_str = false; char str_d = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];
        if (in_str) { out += c; if (c == str_d && !(i+1 < sql.size() && sql[i+1] == str_d)) in_str = false; continue; }
        if (c == '\'' || c == '"') { in_str = true; str_d = c; out += c; continue; }
        if (c == '#') { out += "--"; continue; }
        out += c;
    }
    return out;
}

static std::string fn_dash_to_hash(const std::string& sql) {
    std::string out;
    bool in_str = false; char str_d = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];
        if (in_str) { out += c; if (c == str_d && !(i+1 < sql.size() && sql[i+1] == str_d)) in_str = false; continue; }
        if (c == '\'' || c == '"') { in_str = true; str_d = c; out += c; continue; }
        if (c == '-' && i+1 < sql.size() && sql[i+1] == '-') { out += '#'; ++i; continue; }
        out += c;
    }
    return out;
}

static void cmd_dash_to_block() { apply_transform(fn_dash_to_block); }
static void cmd_block_to_dash() { apply_transform(fn_block_to_dash); }
static void cmd_hash_to_dash()  { apply_transform(fn_hash_to_dash); }
static void cmd_dash_to_hash()  { apply_transform(fn_dash_to_hash); }

// ─── Dialect conversions ──────────────────────────────────────────────────────
// Hub-and-spoke: the rule tables and the canonical-form logic live in
// dialects.cpp. Each menu command fixes the *source* dialect; the *target* is
// whatever is configured in Settings (g_settings.dialect), read at call time.
// A non-capturing lambda decays to the plain function pointer apply_transform
// wants. Converting a dialect to itself is a no-op (handled in convert_dialect).

static void cmd_conv_from_ansi()       { apply_transform([](const std::string& s){ return convert_dialect(s, Dialect::ANSI,       g_settings.dialect); }); }
static void cmd_conv_from_sf()         { apply_transform([](const std::string& s){ return convert_dialect(s, Dialect::Snowflake,  g_settings.dialect); }); }
static void cmd_conv_from_pg()         { apply_transform([](const std::string& s){ return convert_dialect(s, Dialect::PostgreSQL, g_settings.dialect); }); }
static void cmd_conv_from_mssql()      { apply_transform([](const std::string& s){ return convert_dialect(s, Dialect::MSSQL,      g_settings.dialect); }); }
static void cmd_conv_from_mysql()      { apply_transform([](const std::string& s){ return convert_dialect(s, Dialect::MySQL,      g_settings.dialect); }); }
static void cmd_conv_from_sqlite()     { apply_transform([](const std::string& s){ return convert_dialect(s, Dialect::SQLite,     g_settings.dialect); }); }
static void cmd_conv_from_databricks() { apply_transform([](const std::string& s){ return convert_dialect(s, Dialect::Databricks, g_settings.dialect); }); }

static void cmd_settings() { show_settings_dialog(g_npp._nppHandle); }

// ─── About dialog ─────────────────────────────────────────────────────────────

static void cmd_about() { show_about_dialog(g_npp._nppHandle); }

// ─── Help ─────────────────────────────────────────────────────────────────────

// Opens help.txt, which build.cmd deploys next to the DLL, as a
// regular Notepad++ document instead of a MessageBox (readable/searchable/scrollable).
static void cmd_help() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(g_module, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return;
    wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"help.txt");
    SendMessage(g_npp._nppHandle, NPPM_DOOPEN, 0, (LPARAM)path);
}

// ─── Menu construction ────────────────────────────────────────────────────────

static HMENU find_my_menu() {
    HMENU bar = GetMenu(g_npp._nppHandle);
    if (!bar) return nullptr;
    for (int i = GetMenuItemCount(bar) - 1; i >= 0; --i) {
        HMENU top = GetSubMenu(bar, i);
        if (!top) continue;
        for (int j = 0; j < GetMenuItemCount(top); ++j) {
            wchar_t buf[128] = {};
            GetMenuStringW(top, j, buf, 128, MF_BYPOSITION);
            if (_wcsicmp(buf, L"Datamodder SQL Formatter") == 0) return GetSubMenu(top, j);
        }
    }
    return nullptr;
}

static void build_all_submenus(HMENU hMine) {
    // Grab cmdIDs for slots 3-37
    UINT id_fos_on         = g_funcs[3]._cmdID,  id_fos_off        = g_funcs[4]._cmdID;
    UINT id_paste_lines    = g_funcs[6]._cmdID,  id_paste_comma    = g_funcs[7]._cmdID;
    UINT id_paste_comma_sp = g_funcs[8]._cmdID,  id_paste_space    = g_funcs[9]._cmdID;
    UINT id_nl_en          = g_funcs[11]._cmdID, id_en_nl          = g_funcs[12]._cmdID;
    UINT id_fr_en          = g_funcs[13]._cmdID, id_en_fr          = g_funcs[14]._cmdID;
    UINT id_remove_thou    = g_funcs[15]._cmdID;
    UINT id_dq_sq          = g_funcs[16]._cmdID, id_sq_dq          = g_funcs[17]._cmdID;
    UINT id_bt_sq          = g_funcs[18]._cmdID, id_sq_bt          = g_funcs[19]._cmdID;
    UINT id_dq_bt          = g_funcs[20]._cmdID, id_bt_dq          = g_funcs[21]._cmdID;
    UINT id_bool_int       = g_funcs[22]._cmdID, id_int_bool       = g_funcs[23]._cmdID;
    UINT id_lowercase      = g_funcs[24]._cmdID, id_uppercase      = g_funcs[25]._cmdID;
    UINT id_dash_block     = g_funcs[26]._cmdID, id_block_dash     = g_funcs[27]._cmdID;
    UINT id_strip          = g_funcs[28]._cmdID;
    UINT id_hash_dash      = g_funcs[29]._cmdID, id_dash_hash      = g_funcs[30]._cmdID;
    UINT id_from_ansi       = g_funcs[31]._cmdID, id_from_sf        = g_funcs[32]._cmdID;
    UINT id_from_pg         = g_funcs[33]._cmdID, id_from_mssql     = g_funcs[34]._cmdID;
    UINT id_from_mysql      = g_funcs[35]._cmdID, id_from_sqlite    = g_funcs[36]._cmdID;
    UINT id_from_databricks = g_funcs[37]._cmdID;

    // Remove slots 3-37 (35 items) from high to low.
    // After deletion: pos 3 = separator (slot 38), pos 4 = Settings (slot 39),
    // pos 5 = About (slot 40), pos 6 = Help (slot 41).
    for (int i = 37; i >= 3; --i) DeleteMenu(hMine, i, MF_BYPOSITION);

    HMENU hFOS = CreatePopupMenu();
    AppendMenuW(hFOS, MF_STRING, id_fos_on,  L"On");
    AppendMenuW(hFOS, MF_STRING, id_fos_off, L"Off");
    InsertMenuW(hMine, 3, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hFOS, L"Format on Save");

    InsertMenuW(hMine, 4, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

    HMENU hP = CreatePopupMenu();
    AppendMenuW(hP, MF_STRING,    id_paste_lines,    L"Each value on its own line  (a,\nb,\nc)");
    AppendMenuW(hP, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hP, MF_STRING,    id_paste_comma,    L"Comma  (a,b,c)");
    AppendMenuW(hP, MF_STRING,    id_paste_comma_sp, L"Comma + space  (a, b, c)");
    AppendMenuW(hP, MF_STRING,    id_paste_space,    L"Space  (a b c)");
    InsertMenuW(hMine, 5, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hP, L"Paste List");

    InsertMenuW(hMine, 6, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

    HMENU hN = CreatePopupMenu();
    AppendMenuW(hN, MF_STRING,    id_nl_en,       L"NL \x2192 EN  (1.234,56 \x2192 1,234.56)");
    AppendMenuW(hN, MF_STRING,    id_en_nl,       L"EN \x2192 NL  (1,234.56 \x2192 1.234,56)");
    AppendMenuW(hN, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hN, MF_STRING,    id_fr_en,       L"FR/BE \x2192 EN  (1 234,56 \x2192 1,234.56)");
    AppendMenuW(hN, MF_STRING,    id_en_fr,       L"EN \x2192 FR/BE  (1,234.56 \x2192 1 234,56)");
    AppendMenuW(hN, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hN, MF_STRING,    id_remove_thou, L"Remove thousands  (1.234 / 1,234 \x2192 1234)");
    InsertMenuW(hMine, 7, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hN, L"Number Format");

    HMENU hQ = CreatePopupMenu();
    AppendMenuW(hQ, MF_STRING,    id_dq_sq, L"Double \x2192 single  (\" \x2192 ')");
    AppendMenuW(hQ, MF_STRING,    id_sq_dq, L"Single \x2192 double  (' \x2192 \")");
    AppendMenuW(hQ, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hQ, MF_STRING,    id_bt_sq, L"Backtick \x2192 single  (` \x2192 ')");
    AppendMenuW(hQ, MF_STRING,    id_sq_bt, L"Single \x2192 backtick  (' \x2192 `)");
    AppendMenuW(hQ, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hQ, MF_STRING,    id_dq_bt, L"Double \x2192 backtick  (\" \x2192 `)");
    AppendMenuW(hQ, MF_STRING,    id_bt_dq, L"Backtick \x2192 double  (` \x2192 \")");
    InsertMenuW(hMine, 8, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hQ, L"Quotes");

    HMENU hB = CreatePopupMenu();
    AppendMenuW(hB, MF_STRING, id_bool_int, L"TRUE/FALSE \x2192 1/0");
    AppendMenuW(hB, MF_STRING, id_int_bool, L"1/0 \x2192 TRUE/FALSE");
    InsertMenuW(hMine, 9, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hB, L"Boolean");

    HMENU hK = CreatePopupMenu();
    AppendMenuW(hK, MF_STRING, id_lowercase, L"lowercase");
    AppendMenuW(hK, MF_STRING, id_uppercase, L"UPPERCASE");
    InsertMenuW(hMine, 10, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hK, L"Change Casing (excl. string literals)");

    HMENU hC = CreatePopupMenu();
    AppendMenuW(hC, MF_STRING,    id_dash_block, L"-- \x2192 /* */");
    AppendMenuW(hC, MF_STRING,    id_block_dash, L"/* */ \x2192 --");
    AppendMenuW(hC, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hC, MF_STRING,    id_hash_dash,  L"# \x2192 --  (MySQL \x2192 standard)");
    AppendMenuW(hC, MF_STRING,    id_dash_hash,  L"-- \x2192 #  (standard \x2192 MySQL)");
    AppendMenuW(hC, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hC, MF_STRING,    id_strip,      L"Strip comments");
    InsertMenuW(hMine, 11, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hC, L"Comment Style");

    // Pick the source dialect; the target is g_settings.dialect (Settings > Dialect).
    HMENU hD = CreatePopupMenu();
    AppendMenuW(hD, MF_STRING | MF_GRAYED, 0, L"Convert to the dialect set in Settings, from:");
    AppendMenuW(hD, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hD, MF_STRING,    id_from_ansi,       L"ANSI  (generic)");
    AppendMenuW(hD, MF_STRING,    id_from_sf,         L"Snowflake");
    AppendMenuW(hD, MF_STRING,    id_from_pg,         L"PostgreSQL");
    AppendMenuW(hD, MF_STRING,    id_from_mssql,      L"MS SQL  (T-SQL)");
    AppendMenuW(hD, MF_STRING,    id_from_mysql,      L"MySQL / MariaDB");
    AppendMenuW(hD, MF_STRING,    id_from_sqlite,     L"SQLite");
    AppendMenuW(hD, MF_STRING,    id_from_databricks, L"Databricks  (Spark SQL)");
    InsertMenuW(hMine, 12, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hD, L"Dialects");
}

// ─── Plugin interface exports ──────────────────────────────────────────────────

extern "C" {

__declspec(dllexport) const wchar_t* getName() { return L"Datamodder SQL Formatter"; }

__declspec(dllexport) void setInfo(NppData d) { g_npp = d; load_settings(); }

__declspec(dllexport) FuncItem* getFuncsArray(int* n) {
    *n = 42;
    return g_funcs;
}

static void try_build_menu() {
    if (g_menu_built) return;
    HMENU hMine = find_my_menu();
    if (!hMine || GetMenuItemCount(hMine) < 42) return;
    build_all_submenus(hMine);
    g_menu_built = true;
    set_format_on_save_checkmark();
}

__declspec(dllexport) void beNotified(SCNotification* n) {
    try_build_menu();
    if ((n->code == NPPN_FILEBEFORESAVE || n->code == NPPN_FILESAVED) && g_settings.format_on_save) {
        DWORD now = GetTickCount();
        if (now - g_format_save_tick > 1000) {
            g_format_save_tick = now;
            cmd_format_sql();
        }
    }
}

__declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { try_build_menu(); return FALSE; }

__declspec(dllexport) BOOL isUnicode() { return TRUE; }

} // extern "C"

// ─── DLL entry ────────────────────────────────────────────────────────────────

static void init_func(int i, const wchar_t* name, PFUNCPLUGINCMD fn, ShortcutKey* sk = nullptr) {
    wcscpy_s(g_funcs[i]._menuItemName, name);
    g_funcs[i]._pFunc  = fn;
    g_funcs[i]._pShKey = sk;
}

BOOL APIENTRY DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = h;
        DisableThreadLibraryCalls(h);

        init_func(0,  L"Format SQL\tCtrl+Alt+Shift+F", cmd_format_sql,    &g_shortcut_format);
        init_func(1,  L"Minify SQL",                   cmd_minify_sql);
        init_func(2,  L"Format and Copy",              cmd_format_copy);
        init_func(3,  L"On",                           cmd_fos_on);
        init_func(4,  L"Off",                          cmd_fos_off);
        init_func(5,  L"-",                            nullptr);
        init_func(6,  L"Each value on its own line",   cmd_paste_lines);
        init_func(7,  L"Comma",                        cmd_paste_comma);
        init_func(8,  L"Comma + space",                cmd_paste_comma_sp);
        init_func(9,  L"Space",                        cmd_paste_space);
        init_func(10, L"-",                            nullptr);
        init_func(11, L"NL \x2192 EN",                cmd_num_nl_to_en);
        init_func(12, L"EN \x2192 NL",                cmd_num_en_to_nl);
        init_func(13, L"FR/BE \x2192 EN",             cmd_fr_to_en);
        init_func(14, L"EN \x2192 FR/BE",             cmd_en_to_fr);
        init_func(15, L"Remove thousands",             cmd_remove_thousands);
        init_func(16, L"Double \x2192 single",         cmd_quote_dq_to_sq);
        init_func(17, L"Single \x2192 double",         cmd_quote_sq_to_dq);
        init_func(18, L"Backtick \x2192 single",       cmd_quote_bt_to_sq);
        init_func(19, L"Single \x2192 backtick",       cmd_quote_sq_to_bt);
        init_func(20, L"Double \x2192 backtick",       cmd_quote_dq_to_bt);
        init_func(21, L"Backtick \x2192 double",       cmd_quote_bt_to_dq);
        init_func(22, L"TRUE/FALSE \x2192 1/0",        cmd_bool_to_int);
        init_func(23, L"1/0 \x2192 TRUE/FALSE",        cmd_int_to_bool);
        init_func(24, L"lowercase",                    cmd_lowercase_all);
        init_func(25, L"UPPERCASE",                    cmd_uppercase_all);
        init_func(26, L"-- \x2192 /* */",              cmd_dash_to_block);
        init_func(27, L"/* */ \x2192 --",              cmd_block_to_dash);
        init_func(28, L"Strip comments",               cmd_strip_comments);
        init_func(29, L"# \x2192 --",                  cmd_hash_to_dash);
        init_func(30, L"-- \x2192 #",                  cmd_dash_to_hash);
        init_func(31, L"Dialect: from ANSI",           cmd_conv_from_ansi);
        init_func(32, L"Dialect: from Snowflake",       cmd_conv_from_sf);
        init_func(33, L"Dialect: from PostgreSQL",      cmd_conv_from_pg);
        init_func(34, L"Dialect: from MS SQL",          cmd_conv_from_mssql);
        init_func(35, L"Dialect: from MySQL",           cmd_conv_from_mysql);
        init_func(36, L"Dialect: from SQLite",          cmd_conv_from_sqlite);
        init_func(37, L"Dialect: from Databricks",      cmd_conv_from_databricks);
        init_func(38, L"-",                            nullptr);
        init_func(39, L"Settings...",                  cmd_settings);
        init_func(40, L"About",                        cmd_about);
        init_func(41, L"Help",                         cmd_help);
    }
    return TRUE;
}
