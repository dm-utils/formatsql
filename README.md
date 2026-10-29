# Datamodder SQL Formatter (FormatSQL)

A Notepad++ plugin for formatting, minifying and converting SQL.

**Website:** [datamodder.com/en/utils/sql-formatter](https://datamodder.com/en/utils/sql-formatter)
— features, [settings with examples](https://datamodder.com/en/utils/sql-formatter/settings)
and an [online playground](https://datamodder.com/en/utils/sql-formatter/playground)
to try the formatter in your browser.

For 64-bit, 32-bit and ARM64 Notepad++.

## Features

- **Format SQL** (`Ctrl+Alt+Shift+F`) — auto-format with proper spacing and layout
- **Minify SQL** — remove all unnecessary whitespace
- **Format and Copy** — format and copy the result to the clipboard
- **Format on Save** — auto-format `.sql` files (or files with language SQL) on every save; off again after a Notepad++ restart
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

## Release

```
release.cmd
```

Builds `FormatSQL.dll` and zips it with `help.txt` into
`release_staging\FormatSQL.zip` (x64) or `FormatSQL_<arch>.zip`, printing the
SHA-256 the plugin lists need. By default only x64; `release.cmd x64 x86 arm64`
(or the commented line in the script) also builds the Win32 and ARM64
versions. ARM64 needs the "MSVC C++ ARM64/ARM64EC build tools" component of
Visual Studio. `test.cmd x86` runs the tests as a 32-bit program.

## License

MIT — see [LICENSE](LICENSE).
