"""Package the built addon; run from any directory with Python 3."""
from pathlib import Path
import re
import zipfile

root = Path(__file__).resolve().parent.parent
version = re.search(r'set_version\("([^"]+)"\)', (root / 'xmake.lua').read_text()).group(1)
files = {
    'SKSE/Plugins/HairPositionerUI.dll': root / 'build/bin/HairPositionerUI.dll',
    'SKSE/Plugins/HairPositionerUI.ini': root / 'config/HairPositionerUI.ini',
    'Scripts/HPUIBridge.pex': root / 'runtime/Scripts/HPUIBridge.pex',
    'Scripts/Source/HPUIBridge.psc': root / 'scripts/HPUIBridge.psc',
    'README_EN.md': root / 'README.md',
    'README_KO.md': root / 'docs/README_KO.md',
    'TRANSLATING.md': root / 'TRANSLATING.md',
    'CHANGELOG.md': root / 'CHANGELOG.md',
}
for folder, destination in [('translations', 'SKSE/Plugins/HairPositionerUI/translations'), ('licenses', 'licenses')]:
    for source in (root / folder).rglob('*'):
        if source.is_file():
            files[f'{destination}/{source.relative_to(root / folder).as_posix()}'] = source
missing = [str(source.relative_to(root)) for source in files.values() if not source.is_file()]
if missing:
    raise SystemExit('Missing required files: ' + ', '.join(missing))
output = root / 'dist' / f'HairPositionerUI-{version}.zip'
output.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for name, source in sorted(files.items()):
        archive.write(source, name)
with zipfile.ZipFile(output) as archive:
    if archive.testzip() is not None:
        raise SystemExit('Archive validation failed')
print(output)
