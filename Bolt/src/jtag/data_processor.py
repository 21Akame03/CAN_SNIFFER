"""Parse incoming serial lines and update GUI state."""

from __future__ import annotations

import json
from typing import Any, Dict, Optional

from gui import state as st


def process_line(line: str) -> None:
    """Parse an incoming line from the monitor feed."""
    if line is None:
        return
    text = line.strip()
    if not text:
        return

    payload = _parse_json(text)
    if payload is None:
        st.append_log(text)
        return

    msg_type = str(payload.get('type') or '').lower()
    if msg_type == 'can':
        frame = _coerce_can_frame(payload, raw=text)
        if frame is None:
            st.append_log(text)
            return
        st.append_can_frame(frame)
    else:
        st.append_log(text)


def _parse_json(text: str) -> Optional[Dict[str, Any]]:
    try:
        value = json.loads(text)
    except json.JSONDecodeError:
        return None
    if not isinstance(value, dict):
        return None
    return value


def _coerce_can_frame(payload: Dict[str, Any], *, raw: str) -> Optional[Dict[str, Any]]:
    try:
        identifier = int(payload.get('id'))
    except Exception:
        return None
    try:
        ts_us = int(payload.get('ts_us') or payload.get('timestamp_us') or 0)
    except Exception:
        ts_us = 0
    dlc = _coerce_int(payload.get('dlc')) or 0
    ext = bool(payload.get('ext') or payload.get('extended'))
    rtr = bool(payload.get('rtr') or payload.get('remote'))
    data = payload.get('data')

    return {
        'id': identifier,
        'ts_us': ts_us,
        'dlc': dlc,
        'ext': ext,
        'rtr': rtr,
        'data': data,
        'raw': raw,
    }


def _coerce_int(value: Any) -> Optional[int]:
    if value is None:
        return None
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return int(value)
    try:
        text = str(value).strip()
        if not text:
            return None
        base = 16 if text.startswith('0x') else 10
        return int(text, base)
    except Exception:
        return None


__all__ = ['process_line']
