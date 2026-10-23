# Datamodder SQL Formatter (FormatSQL)

A Notepad++ plugin for formatting, minifying and converting SQL.

**Website:** [datamodder.com/en/utils/sql-formatter](https://datamodder.com/en/utils/sql-formatter)
— features, [settings with examples](https://datamodder.com/en/utils/sql-formatter/settings)
and an [online playground](https://datamodder.com/en/utils/sql-formatter/playground)
to try the formatter in your browser.

**64-bit Notepad++ only.** 32-bit is not supported.

## Features

- **Format SQL** (`Ctrl+Alt+Shift+F`) — auto-format with proper spacing and layout
- **Minify SQL** — remove all unnecessary whitespace
- **Format and Copy** — format and copy the result to the clipboard
- **Format on Save** — auto-format the document on every save
- **Paste List** — paste clipboard content (Excel columns/rows) as a quoted, comma-separated SQL list
- **Number Format** — convert numbers between EN, NL and FR/BE decimal/thousands-separator conventions
- **Quotes** — convert between single quotes, double quotes and backticks
- **Boolean** — convert between `TRUE`/`FALSE` and `1`/`0`
- **Change Casing** (excl. string literals) — lowercase/uppercase the document
- **Comment Style** — convert between `--`, `/* */` and `#`, or strip comments
- **Dialects** — convert SQL between ANSI, Snowflake, MS SQL, PostgreSQL, MySQL and Databricks
- **Settings** — configure casing, alignment, structure, spacing, joins, dialect and FQDN qualification rules, with saved profiles

See `src/help.txt` (also available from the plugin's Help menu item once installed) for the full manual.

## Installation

Once listed in Notepad++'s Plugin Admin: **Plugins → Plugins Admin... → Datamodder SQL Formatter → Install**.

Manual install: copy the built `FormatSQL.dll` and `help.txt` into
`<Notepad++ install dir>\plugins\FormatSQL\`.

## Building from source

Requires Visual Studio's C++ build tools (`vcvars64.bat`). From an elevated terminal:

```
build.cmd
```

This compiles the plugin, closes Notepad++, deploys the DLL and help file to
`C:\Program Files\Notepad++\plugins\FormatSQL`, and restarts Notepad++.

## License

MIT — see [LICENSE](LICENSE).
