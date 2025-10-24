"""Runtime bootstrap helpers for the Bolt dashboard."""

from __future__ import annotations

from typing import Optional

from nicegui import ui

from .config import BoltConfig
from gui import app as gui_app


def run(config: Optional[BoltConfig] = None) -> None:
    """Initialise the UI and start the NiceGUI event loop."""
    cfg = config or BoltConfig()
    gui_app.init(cfg)
    ui.run(**cfg.to_ui_kwargs())


__all__ = ['run']
