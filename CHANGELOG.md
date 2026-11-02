# Changelog

This repository consolidates local development that predates its first Git commit. Older packages are preserved in the Development Archive release; the Git history is not a reconstruction of those development sessions.

## 1.2.0 — External translations

- Load English/Korean UI text from UTF-8 CSV files, once at startup.
- Use Skyrim-style full language names: `english`, `korean`, etc. Legacy `en`/`ko` settings still work.
- Allow additional translations without recompiling the DLL.
- Fall back to built-in English for missing or invalid translations.
- Validate UTF-8, CSV quoting, format placeholders, combo separators and the window identifier.
- Include a translator guide and automated translation tests.
- Build and automated tests passed. Gameplay validation is pending.

## 1.1.0 — Bilingual DLL

- Added English/Korean fixed UI text in one DLL, with English as the default.
- Selected language through the addon's INI; game-provided names remained unchanged.

## 1.0.0 — Initial release

- Promoted the existing beta functionality to 1.0.
- Added distribution documentation and source packages.

## 0.3.0 beta 1

- Mouse-wheel adjustment and saved precision levels.
- Per-target undo, up to 64 operations, with grouped drag/wheel gestures.
- Restore the selected target to its editing-session starting values.

## 0.2.0 alpha

- Expanded editing from hair to beard, supported extra head parts, equipped gear and wigs.
- Added target discovery and refresh.

## 0.1.1 alpha

- Addressed initial framework registration/availability problems.

## 0.1.0 alpha

- First compact SKSE Menu Framework bridge for HairPositioner 2.5.
- Position, rotation, scale, pivot and reset controls, with an F3 hotkey.

## Separate font experiments

SKSE Menu Framework 3.18 font-addon experiments were separate from this UI. Some caused reproducible CTDs during font texture creation and were withdrawn. They are not included as downloads here. The UI's language loader does not modify framework fonts or resolve that framework issue.
