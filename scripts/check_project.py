#!/usr/bin/env python3
"""Check the repository layout without requiring Qt or Arduino toolchains."""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def check():
    project = (ROOT / 'gestion_bateaux.pro').read_text(encoding='utf-8').replace('\\\n', ' ')
    for key, extension, directory in [
        ('SOURCES', '.cpp', 'src'), ('HEADERS', '.h', 'src'),
        ('FORMS', '.ui', 'ui'), ('RESOURCES', '.qrc', 'resources'),
    ]:
        match = re.search(r'^' + key + r'\s*\+=\s*(.+)$', project, re.M)
        require(match is not None, f'Missing qmake section: {key}')
        paths = match.group(1).split()
        require(len(paths) == len(set(paths)), f'Duplicate {key} entries')
        expected = {p.relative_to(ROOT).as_posix() for p in (ROOT / directory).rglob('*' + extension)}
        require(set(paths) == expected, f'{key} differs from files on disk')
        require(all((ROOT / path).is_file() for path in paths), f'Missing {key} file')
        print(f'{key}: {len(paths)} paths verified')

    aliases = set()
    for path in (ROOT / 'resources').rglob('*'):
        if path.suffix not in {'.svg', '.qrc'}:
            continue
        tree = ET.parse(path)
        if path.suffix == '.qrc':
            for resource in tree.getroot():
                for entry in resource:
                    require((path.parent / entry.text).is_file(), f'Missing resource: {entry.text}')
                    aliases.add(':' + resource.get('prefix', '').rstrip('/') + '/' + entry.get('alias', entry.text))
    for path in (ROOT / 'ui').glob('*.ui'):
        ET.parse(path)
    print('UI, QRC, and SVG XML parsed; QRC files exist')

    include_paths = []
    for line in project.splitlines():
        if line.startswith('INCLUDEPATH +='):
            include_paths.extend(ROOT / item.removeprefix('$$PWD/') for item in line.split('+=', 1)[1].split())
    require(include_paths and all(path.is_dir() for path in include_paths), 'Invalid INCLUDEPATH')
    references = set()
    for path in list((ROOT / 'src').rglob('*.cpp')) + list((ROOT / 'src').rglob('*.h')) + list((ROOT / 'ui').glob('*.ui')):
        text = path.read_text(encoding='utf-8')
        references.update(re.findall(r'(?<![A-Za-z]):/(?!/)[^\s"<>]+', text))
        for name in re.findall(r'#include "([^"]+)"', text):
            if name == 'ui_fishingport.h':
                continue
            require(any((folder / name).is_file() for folder in [path.parent] + include_paths), f'Unresolved include: {name}')
    require(not references - aliases, f'Unresolved resource aliases: {references - aliases}')
    print(f'Local includes and {len(references)} literal resource references verified')

    sketches = list((ROOT / 'firmware').rglob('*.ino'))
    require(len(sketches) == 3, 'Expected three firmware sketches')
    for path in sketches:
        require(path.stem == path.parent.name, f'Arduino folder/sketch mismatch: {path}')
    print('Three Arduino sketch names match their folders')
    print('Structural checks passed. Compilation and runtime are not tested by this script.')


if __name__ == '__main__':
    try:
        check()
    except (ValueError, OSError, ET.ParseError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
