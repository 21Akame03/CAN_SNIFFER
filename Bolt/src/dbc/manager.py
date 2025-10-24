"""Load DBC files and decode CAN frames."""

from __future__ import annotations

import json
from dataclasses import dataclass
from io import StringIO
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

import cantools
from cantools.database import Database
from cantools.database.can import Message
from cantools.database.errors import DecodeError


@dataclass
class _DbcEntry:
    name: str
    database: Database

    @property
    def message_count(self) -> int:
        return len(self.database.messages)


_loaded: List[_DbcEntry] = []
_message_index: Dict[Tuple[int, bool], Message] = {}


def load_dbc(name: str, data: bytes) -> Tuple[bool, str]:
    """Load a DBC file from raw bytes."""
    text = _coerce_text(data)
    if text is None:
        return False, f'Unable to decode {name}: unsupported encoding'
    try:
        database = cantools.database.load(StringIO(text))
    except Exception as exc:  # pragma: no cover - defensive
        return False, f'Failed to load {name}: {exc}'

    entry = _DbcEntry(name=name, database=database)
    _loaded.append(entry)

    collisions: List[str] = []
    for message in database.messages:
        key = (message.frame_id, bool(message.is_extended_frame))
        if key in _message_index:
            collisions.append(f'0x{message.frame_id:08X}')
        _message_index[key] = message

    notice = f'Loaded {name} ({entry.message_count} messages)'
    if collisions:
        notice += f'; overriding IDs: {", ".join(sorted(set(collisions)))}'
    return True, notice


def list_dbcs() -> List[Dict[str, object]]:
    """Return metadata about the currently loaded DBC files."""
    return [
        {'name': entry.name, 'messages': entry.message_count}
        for entry in _loaded
    ]


def clear_dbcs() -> None:
    """Remove all loaded DBC databases."""
    _loaded.clear()
    _message_index.clear()


def has_dbcs() -> bool:
    return bool(_loaded)


def decode_frame(identifier: int, data_bytes: Sequence[int], *, extended: bool) -> Optional[Dict[str, object]]:
    """Decode a CAN frame using the loaded DBC databases."""
    if not _message_index:
        return None

    message = _message_index.get((int(identifier), bool(extended)))
    if message is None:
        # fall back to same ID with unknown frame type if no direct match
        message = _message_index.get((int(identifier), not bool(extended)))
    if message is None:
        return None

    payload = _to_payload(data_bytes, message.length)
    if payload is None:
        return None

    try:
        decoded = message.decode(payload, decode_choices=True, scaling=True)
    except DecodeError:
        return None
    except Exception:  # pragma: no cover - defensive
        return None

    signals = _format_signals(message, decoded)
    if not signals:
        return None
    return {
        'message_name': message.name,
        'signals': signals,
    }


def _format_signals(message: Message, decoded: Dict[str, object]) -> List[Tuple[str, str]]:
    result: List[Tuple[str, str]] = []
    for name, value in decoded.items():
        signal = _get_signal(message, name)
        text = _stringify_value(value, signal.unit if signal else None)
        result.append((name, text))
    return result


def _get_signal(message: Message, name: str):
    try:
        return message.get_signal_by_name(name)
    except KeyError:
        return None


def _stringify_value(value: object, unit: Optional[str]) -> str:
    if isinstance(value, float):
        magnitude = abs(value)
        if magnitude and (magnitude >= 1000 or magnitude < 0.001):
            text = f'{value:.3e}'
        else:
            text = f'{value:.3f}'.rstrip('0').rstrip('.')
    elif isinstance(value, (bytes, bytearray)):
        text = value.hex().upper()
    elif isinstance(value, dict):
        text = json.dumps(value, separators=(',', ':'))
    elif isinstance(value, Iterable) and not isinstance(value, (str, bytes, bytearray)):
        text = ','.join(str(v) for v in value)
    else:
        text = str(value)
    if unit:
        return f'{text} {unit}'
    return text


def _to_payload(data_bytes: Sequence[int], message_length: Optional[int]) -> Optional[bytes]:
    try:
        values = [(int(b) & 0xFF) for b in data_bytes]
    except Exception:
        return None
    if message_length is None:
        message_length = len(values)
    if message_length > len(values):
        values = values + [0] * (message_length - len(values))
    elif message_length > 0 and len(values) > message_length:
        values = values[:message_length]
    return bytes(values)


def _coerce_text(data: bytes) -> Optional[str]:
    for encoding in ('utf-8-sig', 'utf-8', 'latin-1'):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode('utf-8', errors='ignore') if data else ''


__all__ = ['load_dbc', 'list_dbcs', 'clear_dbcs', 'has_dbcs', 'decode_frame']
