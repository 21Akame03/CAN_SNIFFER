"""Command-line interface helpers for the Bolt application."""

from __future__ import annotations

import argparse
import os
from typing import Optional, Sequence

from .config import BoltConfig
from .runtime import run

_LOG_LEVELS = ['critical', 'error', 'warning', 'info', 'debug', 'trace']


def _truthy(value: Optional[str]) -> Optional[bool]:
    if value is None:
        return None
    text = value.strip().lower()
    if text in {'1', 'true', 'yes', 'on'}:
        return True
    if text in {'0', 'false', 'no', 'off'}:
        return False
    return None


def _resolve_flag(env_name: str, default: bool, cli_value: Optional[bool]) -> bool:
    if cli_value is not None:
        return cli_value
    env_value = _truthy(os.getenv(env_name))
    if env_value is not None:
        return env_value
    return default


def _resolve_text(env_name: str, default: str, cli_value: Optional[str]) -> str:
    if cli_value:
        return cli_value
    env_value = os.getenv(env_name)
    if env_value:
        return env_value
    return default


def _resolve_port(env_name: str, default: int, cli_value: Optional[int]) -> int:
    if cli_value is not None:
        return cli_value
    env_value = os.getenv(env_name)
    if env_value is not None:
        try:
            return int(env_value)
        except ValueError:
            pass
    return default


def create_parser() -> argparse.ArgumentParser:
    """Build the argument parser."""
    parser = argparse.ArgumentParser(description='Bolt CAN monitor dashboard')
    parser.add_argument('--host', help='Host/IP address to bind the web server to')
    parser.add_argument('--port', type=int, help='TCP port to bind the web server to')
    parser.add_argument('--title', help='Window and page title')
    parser.add_argument('--log-level', choices=_LOG_LEVELS, help='Uvicorn logging level')

    parser.set_defaults(reload=None, fastapi_docs=None, show=None)

    parser.add_argument('--reload', dest='reload', action='store_true', help='Enable code auto-reload (development only)')
    parser.add_argument('--no-reload', dest='reload', action='store_false', help='Disable code auto-reload')

    docs_group = parser.add_mutually_exclusive_group()
    docs_group.add_argument('--docs', dest='fastapi_docs', action='store_true', help='Expose FastAPI docs at /docs')
    docs_group.add_argument('--no-docs', dest='fastapi_docs', action='store_false', help='Hide FastAPI docs (default)')

    browser_group = parser.add_mutually_exclusive_group()
    browser_group.add_argument('--browser', dest='show', action='store_true', help='Open a browser window on launch')
    browser_group.add_argument('--no-browser', dest='show', action='store_false', help='Do not open a browser window automatically')

    return parser


def build_config(args: argparse.Namespace, *, defaults: Optional[BoltConfig] = None) -> BoltConfig:
    """Translate CLI arguments into a :class:`BoltConfig`."""
    defaults = defaults or BoltConfig()
    host = _resolve_text('BOLT_HOST', defaults.host, getattr(args, 'host', None))
    port = _resolve_port('BOLT_PORT', defaults.port, getattr(args, 'port', None))
    title = _resolve_text('BOLT_TITLE', defaults.title, getattr(args, 'title', None))
    log_level = _resolve_text('BOLT_LOG_LEVEL', defaults.uvicorn_logging_level, getattr(args, 'log_level', None))
    reload_flag = _resolve_flag('BOLT_RELOAD', defaults.reload, getattr(args, 'reload', None))
    docs_flag = _resolve_flag('BOLT_FASTAPI_DOCS', defaults.fastapi_docs, getattr(args, 'fastapi_docs', None))
    show_flag = _resolve_flag('BOLT_SHOW_BROWSER', defaults.show, getattr(args, 'show', None))

    return BoltConfig(
        title=title,
        host=host,
        port=port,
        reload=reload_flag,
        show=show_flag,
        uvicorn_logging_level=log_level,
        fastapi_docs=docs_flag,
    )


def main(argv: Optional[Sequence[str]] = None) -> None:
    """CLI entrypoint used by both ``python -m bolt`` and ``src/main.py``."""
    parser = create_parser()
    args = parser.parse_args(argv)
    config = build_config(args)
    run(config)


__all__ = ['create_parser', 'build_config', 'main']
