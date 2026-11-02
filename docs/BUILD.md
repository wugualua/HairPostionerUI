# Build and package

## Dependencies

- Windows x64, MSVC with C++23 support, and Windows SDK. The current local build was verified with Visual Studio Build Tools 2026.
- xmake 3.x. The project declares xmake 3.1.0 as its minimum.
- CommonLibSSE-NG from alandtse/CommonLibVR, pinned as a submodule.
- xmake resolves DirectXMath, DirectXTK and spdlog. Network access is required on a new machine.
- Python 3 for the packaging script only.

```text
git clone --recurse-submodules https://github.com/gamingforfun625-alt/HairPostionerUI.git
cd HairPostionerUI
xmake f -m releasedbg -a x64 --vs=2026 -y
xmake build -j6 HairPositionerUI
xmake build LanguageTests
xmake build EditorHistoryTests
build\tests\LanguageTests.exe translations
build\tests\EditorHistoryTests.exe
python tools/package.py
```

Tests run from the repository root. The language test uses `build/tests` for a temporary fixture. Output is `build/bin/HairPositionerUI.dll`; the installation ZIP is written to `dist/`. SE and AE are enabled by the project's CommonLib configuration; VR is disabled. Runtime validation is a separate step.

## Papyrus bridge

`scripts/HPUIBridge.psc` and `runtime/Scripts/HPUIBridge.pex` are the addon's own source and precompiled bridge. The bridge has not changed since 1.0.0; the current package reuses the same PEX. A C++-only change does not require recompiling it.

To change the bridge, use the Skyrim Creation Kit Papyrus compiler with the matching HairPositioner 2.5 script declarations, SKSE script sources and RaceMenu/NiOverride declarations. See [the original Korean build notes](BUILD_KO.md). The project does not redistribute dependencies' script sources or compilers. Always distribute a matching DLL and bridge PEX, and validate a changed bridge separately.

## Release contents

The packaging script includes the addon DLL, bridge PEX and its source, INI defaults, CSV translations, documentation and third-party notices. It excludes original HairPositioner, SKSE Menu Framework, RaceMenu, font addons, build caches and debug symbols. Publish matching source alongside a binary release.
