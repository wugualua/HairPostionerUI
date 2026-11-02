# 빌드

HairPositioner UI 소스는 GPL-3.0-or-later입니다.

- CommonLibSSE-NG: https://github.com/alandtse/CommonLibVR, ng 브랜치
- 확인한 커밋: 39f9d07a6ffabea8fb559eee87ab7d27cd463e8a
- SKSE Menu Framework API: https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API
  include/SKSEMenuFramework.h와 LGPL-2.1 라이선스를 포함합니다.
- xmake 3.x, MSVC x64, C++23. 이 로컬 빌드는 Visual Studio 2026을 사용했습니다.
- xmake 의존성: directxmath 2024.02, directxtk 24.2.0, spdlog 1.16.0

CommonLib 소스를 lib/commonlibsse-ng에 둡니다. 의존성 소스는 수정하지 않았습니다.

```
xmake f -m releasedbg -a x64 --vs=2026 -y
xmake build -j6 HairPositionerUI
```

결과는 build/bin/HairPositionerUI.dll입니다.
HPUIBridge.psc는 Creation Kit PapyrusCompiler로 컴파일합니다.
원본 HairPositioner 2.5의 HairPositioner.psc, RaceMenu BSA에 포함된 NiOverride.psc,
게임/SKSE의 스크립트 소스를 import 경로에 넣습니다.
MO2에 있는 오래된 loose NiOverride.psc에는 BodyMorph 함수가 없을 수 있으므로,
실제 RaceMenu BSA의 소스를 우선하는 것이 필요했습니다. 설치된 소스 파일은 수정하지 않았습니다.

package/SKSE/Plugins/HairPositionerUI.dll과 package/Scripts/HPUIBridge.pex를 배포합니다.
HairPositioner.psc/pex, NiOverride.psc/pex 및 기존 DLL은 이 패키지에 재배포하지 않습니다.


## 공개 저장소 빌드

최신 영문 안내는 BUILD.md를 참고하세요. git clone --recurse-submodules로 CommonLib 의존성도 받으세요.
기존 자체 브리지 PEX는 runtime/Scripts에 포함되며 C++만 수정할 때 다시 컴파일할 필요가 없습니다.
번역은 translations/english.csv와 korean.csv에 있습니다.
패키징은 python tools/package.py로 실행하고 dist/에 생성합니다.
