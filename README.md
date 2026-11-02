# HairPositioner UI 1.2.1

A compact SKSE Menu Framework editor for HairPositioner 2.5. Adjust player hair, beard, supported extra head parts and equipped wigs/gear without opening RaceMenu.

[Downloads](https://github.com/wugualua/HairPostionerUI/releases) · [한국어 안내](docs/README_KO.md) · [Changelog](CHANGELOG.md) · [Build instructions](docs/BUILD.md) · [Translation guide](TRANSLATING.md)

## Install and use
Install HairPositioner 2.5 and its requirements, RaceMenu/NiOverride, SKSE Menu Framework, and SKSE64/Address Library for your game runtime. Install this ZIP as a separate mod below HairPositioner in MO2. No ESP is included. Replace the previous UI version; keep the original HairPositioner enabled.

Press F3 to toggle the editor, or use Mod Control Panel > HairPositioner > Parts Editor. Select a target. Drag a value or use the wheel over it; Ctrl+click allows direct input. Fine/Normal/Coarse control the step size. Undo and Restore session start apply to the selected target. Reset restores defaults. Hotkeys can be changed or disabled in the UI. Save the game to preserve adjustments. Undo history is session-only.

## Background refresh
Periodic current-value and target-list reads keep the controls available. `Applying values...` is shown only for pending edits, not read-only refreshes. Writes are still processed one at a time.

## Language
Edit `SKSE/Plugins/HairPositionerUI.ini` and restart the game:

```ini
[General]
Language=english
```

Use `korean` for Korean. Names follow Skyrim's full language-name style (`english`, `korean`, `japanese`, `french`, etc.), not ISO abbreviations. Selection is explicit: this addon does not change or automatically follow Skyrim.ini. Legacy `en` and `ko` settings still work.

Translations are external UTF-8 CSV files in `SKSE/Plugins/HairPositionerUI/translations/`. English and Korean are included. A missing file or missing/invalid entry falls back to built-in English. See `TRANSLATING.md` to add a language without recompiling. Existing hotkey and precision settings remain compatible.

Game-provided item names are not translated. Korean and other non-Latin text still require the appropriate SKSE Menu Framework font configuration. This update does not change fonts or fix the previously reproduced SKSEMF 3.18 font-size CTD.

## Compatibility and validation
Targets SE and AE; VR is disabled. Prior gameplay feedback is from 1.5.97. 1.6.1170 runtime verification remains outstanding. Version 1.2.1 is not yet tested in game. Compile and automated checks do not prove runtime compatibility.

The Papyrus bridge and editing behavior are unchanged from 1.0.0. Ship the DLL and included HPUIBridge.pex together.

## Credits and license

AI development assistance: **OpenAI Codex** — implementation, English/Korean localization, debugging assistance, automated tests, and release documentation. Project direction and in-game testing are provided by the maintainer. This credit does not imply OpenAI endorsement.

HairPositioner: [Iwillbewarlock](https://github.com/Iwillbewarlock/HairPositioner).

SKSE Menu Framework/API: [QTR-Modding](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API).

CommonLibSSE-NG: [alandtse and contributors](https://github.com/alandtse/CommonLibVR).
SKSE and RaceMenu/NiOverride are required separately.
UI source: GPL-3.0-or-later. Third-party licenses are included in licenses/. Matching source is supplied separately. Original mod runtime files are not redistributed.
