#!/usr/bin/env python3
"""Helper to build standalone executables with PyInstaller."""

from __future__ import annotations

import argparse
import platform
import sys
from pathlib import Path
from typing import Iterable, List, Tuple

_SYSTEM_LABELS = {
    'Windows': 'windows',
    'Darwin': 'mac',
    'Linux': 'linux',
}


def _format_add_data(src: str, dest: str) -> str:
    sep = ';' if platform.system() == 'Windows' else ':'
    return f'{src}{sep}{dest}'


def _ensure_pyinstaller() -> Tuple[object, object]:
    try:
        import PyInstaller.__main__ as pyinstaller_main  # type: ignore
        from PyInstaller.utils.hooks import collect_data_files, collect_submodules  # type: ignore
    except ImportError as exc:  # pragma: no cover - dev helper
        print('PyInstaller is required. Install it with "pip install pyinstaller".', file=sys.stderr)
        raise SystemExit(1) from exc
    return pyinstaller_main, collect_data_files, collect_submodules


def _collect_runtime_artifacts(collect_data_files, collect_submodules) -> Tuple[List[Tuple[str, str]], List[str]]:
    # NiceGUI bundles a fair amount of static content (JS/CSS) that needs to ship with the binary.
    raw_data: List[Tuple[str, str]] = collect_data_files('nicegui', include_py_files=False)
    data: List[Tuple[str, str]] = []
    seen: set[Tuple[str, str]] = set()
    for item in raw_data:
        if item in seen:
            continue
        seen.add(item)
        data.append(item)
    # Ensure FastAPI and friends are fully discovered when freezing.
    hidden: List[str] = []
    for package in ('nicegui', 'fastapi', 'starlette', 'uvicorn', 'pydantic', 'pydantic_settings'):
        try:
            hidden.extend(collect_submodules(package))
        except Exception:
            continue
    return data, sorted(set(hidden))


def _build_arguments(args: argparse.Namespace, *, main_script: Path, data_files: Iterable[Tuple[str, str]], hidden_imports: Iterable[str], dist_dir: Path, work_dir: Path, spec_dir: Path) -> List[str]:
    cmd: List[str] = [
        '--name', args.name,
        '--distpath', str(dist_dir),
        '--workpath', str(work_dir),
        '--specpath', str(spec_dir),
        '--noconfirm',
    ]
    if not args.no_clean:
        cmd.append('--clean')
    if args.onefile:
        cmd.append('--onefile')
    if args.windowed:
        cmd.append('--windowed')

    for src, dest in data_files:
        cmd.extend(['--add-data', _format_add_data(src, dest)])
    for hidden in hidden_imports:
        cmd.extend(['--hidden-import', hidden])

    cmd.append(str(main_script))
    return cmd


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description='Build PyInstaller bundles for the Bolt dashboard')
    system = platform.system()
    parser.add_argument('--name', default='bolt-can-monitor', help='Name for the generated executable')
    parser.add_argument('--platform-tag', default=_SYSTEM_LABELS.get(system, system.lower()), choices=sorted(set(_SYSTEM_LABELS.values())), help='Output directory tag (defaults to the current OS)')
    parser.add_argument('--onefile', action='store_true', help='Build a single-file executable')
    parser.add_argument('--windowed', action='store_true', help='Hide the console window (Windows/macOS)')
    parser.add_argument('--no-clean', action='store_true', help='Skip PyInstaller cleanup step')
    parser.add_argument('--dry-run', action='store_true', help='Print the PyInstaller command without executing it')
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    pyinstaller_main, collect_data_files, collect_submodules = _ensure_pyinstaller()

    project_root = Path(__file__).resolve().parents[1]
    src_dir = project_root / 'src'
    main_script = src_dir / 'main.py'
    if not main_script.is_file():
        print(f'Unable to locate main script at {main_script}', file=sys.stderr)
        raise SystemExit(1)

    build_root = project_root / 'build' / 'pyinstaller'
    work_dir = build_root / 'work'
    spec_dir = build_root / 'spec'
    dist_dir = project_root / 'dist' / args.platform_tag

    for path in (work_dir, spec_dir, dist_dir):
        path.mkdir(parents=True, exist_ok=True)

    data_files, hidden_imports = _collect_runtime_artifacts(collect_data_files, collect_submodules)
    cmd = _build_arguments(args, main_script=main_script, data_files=data_files, hidden_imports=hidden_imports, dist_dir=dist_dir, work_dir=work_dir, spec_dir=spec_dir)

    print('Running PyInstaller with:')
    print('  pyinstaller ' + ' '.join(cmd))

    if args.dry_run:
        return

    pyinstaller_main.run(cmd)
    print(f'Build complete. Output available in {dist_dir}')


if __name__ == '__main__':
    main()
