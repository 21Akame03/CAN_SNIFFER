"""Configuration structures for the Bolt app runtime."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict


@dataclass(slots=True)
class BoltConfig:
    """Runtime options for launching the Bolt dashboard."""

    title: str = 'Bolt CAN Monitor'
    host: str = '127.0.0.1'
    port: int = 8075
    reload: bool = False
    show: bool = True
    uvicorn_logging_level: str = 'warning'
    fastapi_docs: bool = False

    def to_ui_kwargs(self) -> Dict[str, object]:
        """Translate the config into keyword arguments for ``ui.run``."""
        return {
            'title': self.title,
            'host': self.host,
            'port': self.port,
            'reload': self.reload,
            'show': self.show,
            'uvicorn_logging_level': self.uvicorn_logging_level,
            'fastapi_docs': self.fastapi_docs,
        }


__all__ = ['BoltConfig']
