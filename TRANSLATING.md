# Add a translation (no DLL build required)

1. Copy `SKSE/Plugins/HairPositionerUI/translations/english.csv` to a file with a Skyrim-style language name, e.g. `japanese.csv`, `french.csv`, `german.csv`, `italian.csv`, `spanish.csv`, `russian.csv`, `polish.csv`, or `chinese.csv`. Use lowercase filenames for consistency. English and Korean are shipped as `english.csv` and `korean.csv`.
2. Edit the `text` column only. Keep `key,text` as the header and keep every `ui.NNN` key unchanged. These identifiers are stable; new releases append new keys rather than renumbering them.
3. Save as **UTF-8 CSV** (BOM optional). Spreadsheet programs must export CSV, not XLSX or a local ANSI encoding. Quote fields containing commas/newlines/quotes. Double quotes inside a quoted CSV field are written twice.
4. Set `[General] Language=japanese` (or the chosen filename stem) in `SKSE/Plugins/HairPositionerUI.ini` and restart Skyrim. This is an explicit setting, not automatic game-language detection. Unrecognized filenames simply fall back to English; legacy `en`/`ko` map to `english`/`korean`.
5. Distribute the translated CSV at the same Data-relative path. The UI DLL, original mods, and fonts do not need to be bundled with a translation. Users select the language in their own INI.

## Formatting rules

- Preserve `%s`, `%u`, `%.3f`, `%.2f` exactly, including their order. Unsafe or mismatched formatting is rejected and the built-in English entry is shown.
- Two combo-list entries contain the literal CSV text `\0` (one backslash followed by zero). Preserve all three separators, including the final one. Do not enter actual binary NUL bytes.
- Supported text escapes are backslash + `0`, `n`, `r`, `t`, and backslash + backslash. They mean a list separator, newline, carriage return, tab, and literal backslash respectively. Unknown escapes are rejected.
- The editor title ends with `###HairPositionerUI`. Preserve that exact suffix; translate only the visible prefix. Do not add `##` IDs to other labels.
- Keep keys unique. The first valid entry for a key is used; duplicate, unknown, empty, or invalid entries are ignored. Missing keys use built-in English, not another external file.
- Quoted CSV values may contain actual newlines. Malformed CSV, invalid UTF-8 or a file over 4 MiB causes the whole file to fall back to English.
- Item/part names supplied by the game and keyboard names supplied by Windows are not in this catalog.

Check `HairPositionerUI.log` for the selected language, CSV load status, and loaded/rejected counts. Changes are read once at startup. Font glyph support is configured in SKSE Menu Framework; this translation loader never changes the shared font atlas.

Project license: GPL-3.0-or-later. Keep applicable notices when redistributing modified project files.
