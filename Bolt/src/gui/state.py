"""Shared UI state and update helpers for the Bolt dashboard."""

from __future__ import annotations

import re
import time
from collections import Counter, deque
from dataclasses import dataclass, field
from typing import Any, Callable, Deque, Dict, Iterable, List, Optional, Tuple
from nicegui import ui
from nicegui.elements.dark_mode import DarkMode

from dbc import decode_frame as dbc_decode, list_dbcs

# Maximum number of frames kept in memory and displayed in the table
_MAX_FRAMES = 500
# Maximum age (in milliseconds) to keep frames once fresher data arrives
_MAX_FRAME_AGE_MS = 60_000
# Maximum number of log lines kept in the console
_MAX_LOG_LINES = 400


@dataclass
class CanFrame:
    seq: int
    ts_us: int
    relative_ms: float
    identifier: int
    extended: bool
    rtr: bool
    dlc: int
    data_bytes: List[int]
    raw: str
    message_name: Optional[str] = None
    decoded_signals: List[Tuple[str, str]] = field(default_factory=list)

    @property
    def id_hex(self) -> str:
        width = 8 if self.extended else 3
        return f"0x{self.identifier:0{width}X}"

    @property
    def data_hex_pairs(self) -> List[str]:
        if not self.data_bytes:
            return []
        return [f"{b:02X}" for b in self.data_bytes]

    @property
    def flags_label(self) -> str:
        flags: List[str] = []
        if self.extended:
            flags.append('EXT')
        if self.rtr:
            flags.append('RTR')
        return ', '.join(flags) if flags else '—'

    @property
    def message_label(self) -> str:
        if self.message_name:
            return self.message_name
        return '—'

    @property
    def decoded_label(self) -> str:
        if not self.decoded_signals:
            return '—'
        return '; '.join(f"{name}={value}" for name, value in self.decoded_signals)


_frame_history: Deque[CanFrame] = deque(maxlen=_MAX_FRAMES)
_frame_seq: int = 0
_start_ts_us: Optional[int] = None
_last_frame_monotonic: Optional[float] = None
_filter_text: str = ''
_id_counts: Counter[int] = Counter()

_table_updater: Optional[Callable[[List[Dict[str, Any]]], None]] = None
_chart_updater: Optional[Callable[[List[str], List[int]], None]] = None
_log_writer: Optional[Callable[[str], None]] = None
_log_clearer: Optional[Callable[[], None]] = None
_connection_labels: List[Any] = []
_connection_state: Dict[str, Any] = {
    'connected': False,
    'message': 'Disconnected',
}
_log_buffer: Deque[str] = deque(maxlen=_MAX_LOG_LINES)
_dark_mode_controller: Optional[DarkMode] = None
_dark_mode_enabled: bool = True
_dbc_status_label: Optional[Any] = None
_dbc_container: Optional[Any] = None


def register_table_updater(fn: Callable[[List[Dict[str, Any]]], None]) -> None:
    global _table_updater
    _table_updater = fn
    _push_table_update()


def register_chart_updater(fn: Callable[[List[str], List[int]], None]) -> None:
    global _chart_updater
    _chart_updater = fn
    _push_chart_update()


def register_log(write: Callable[[str], None], clear: Callable[[], None]) -> None:
    global _log_writer, _log_clearer
    _log_writer = write
    _log_clearer = clear
    # Flush existing backlog into the UI when the logger attaches
    if _log_writer:
        for line in _log_buffer:
            _log_writer(line)


def register_connection_indicator(label: Any) -> None:
    _connection_labels.append(label)
    _apply_connection_state()


def register_dbc_listing(status_label: Any, container: Any) -> None:
    global _dbc_status_label, _dbc_container
    _dbc_status_label = status_label
    _dbc_container = container
    refresh_dbc_listing()


def set_connection_state(connected: bool, message: str) -> None:
    _connection_state['connected'] = bool(connected)
    _connection_state['message'] = str(message)
    _apply_connection_state()


def _apply_connection_state() -> None:
    color = '#10B981' if _connection_state['connected'] else '#EF4444'
    text = _connection_state['message']
    for label in list(_connection_labels):
        try:
            label.set_text(text)
            label.style(f'color: {color}')
        except Exception:
            try:
                label.text = text
                label.style(f'color: {color}')
            except Exception:
                continue


def _apply_dark_mode() -> None:
    if not _dark_mode_controller:
        return
    if _dark_mode_enabled:
        _dark_mode_controller.enable()
    else:
        _dark_mode_controller.disable()


def seconds_since_last_frame() -> Optional[float]:
    if _last_frame_monotonic is None:
        return None
    return max(0.0, time.monotonic() - _last_frame_monotonic)


def append_can_frame(frame: Dict[str, Any]) -> None:
    global _frame_seq, _start_ts_us, _last_frame_monotonic

    try:
        ts_us = int(frame.get('ts_us') or frame.get('timestamp_us') or 0)
    except Exception:
        ts_us = 0
    if ts_us < 0:
        ts_us = 0
    if _start_ts_us is None and ts_us:
        _start_ts_us = ts_us
    base_ts = _start_ts_us or ts_us or 0
    relative_ms = float(ts_us - base_ts) / 1000.0 if base_ts else 0.0

    try:
        identifier = int(frame.get('id') or 0)
    except Exception:
        identifier = 0
    extended = bool(frame.get('ext'))
    rtr = bool(frame.get('rtr'))
    try:
        dlc = int(frame.get('dlc') or 0)
    except Exception:
        dlc = 0

    data_bytes = _coerce_data_bytes(frame.get('data'), dlc)
    raw = str(frame.get('raw') or frame.get('raw_line') or frame.get('raw_text') or '')

    decoded = dbc_decode(identifier, data_bytes, extended=extended)
    message_name, decoded_signals = _extract_decoded(decoded)

    _frame_seq += 1
    can_frame = CanFrame(
        seq=_frame_seq,
        ts_us=ts_us,
        relative_ms=relative_ms,
        identifier=identifier,
        extended=extended,
        rtr=rtr,
        dlc=dlc,
        data_bytes=data_bytes,
        raw=raw,
        message_name=message_name,
        decoded_signals=decoded_signals,
    )

    _frame_history.append(can_frame)
    _id_counts[identifier] += 1
    _prune_stale_frames(can_frame.relative_ms)
    _last_frame_monotonic = time.monotonic()
    _push_table_update()
    _push_chart_update()


def _coerce_data_bytes(raw: Any, dlc: int) -> List[int]:
    if raw is None:
        return []
    if isinstance(raw, (bytes, bytearray)):
        values = list(raw)
    elif isinstance(raw, Iterable) and not isinstance(raw, (str, dict)):
        try:
            values = [int(v) & 0xFF for v in raw]
        except Exception:
            values = []
    else:
        text = str(raw)
        if not text:
            values = []
        else:
            values = []
            for token in re.findall(r'0x[0-9a-fA-F]+|[0-9a-fA-F]{1,}', text):
                chunk = token.strip()
                if not chunk:
                    continue
                if chunk.lower().startswith('0x'):
                    chunk = chunk[2:]
                if not chunk:
                    continue
                if len(chunk) % 2:
                    chunk = '0' + chunk
                for i in range(0, len(chunk), 2):
                    try:
                        values.append(int(chunk[i : i + 2], 16) & 0xFF)
                    except ValueError:
                        break
    if dlc > 0:
        return values[:dlc]
    return values[:8]


def _prune_stale_frames(reference_ms: float) -> None:
    if reference_ms is None or reference_ms <= 0:
        return
    cutoff = reference_ms - _MAX_FRAME_AGE_MS
    if cutoff <= 0:
        return

    while _frame_history and _frame_history[0].relative_ms < cutoff:
        old_frame = _frame_history.popleft()
        count = _id_counts.get(old_frame.identifier, 0)
        if count > 1:
            _id_counts[old_frame.identifier] = count - 1
        elif old_frame.identifier in _id_counts:
            del _id_counts[old_frame.identifier]


def set_filter(text: str) -> None:
    global _filter_text
    _filter_text = text.strip().lower()
    _push_table_update()


def register_dark_mode_controller(controller: DarkMode) -> None:
    global _dark_mode_controller
    _dark_mode_controller = controller
    _apply_dark_mode()


def set_dark_mode(enabled: bool) -> None:
    global _dark_mode_enabled
    _dark_mode_enabled = bool(enabled)
    _apply_dark_mode()


def toggle_dark_mode() -> bool:
    set_dark_mode(not _dark_mode_enabled)
    return _dark_mode_enabled


def dark_mode_enabled() -> bool:
    return _dark_mode_enabled


def clear_frames() -> None:
    global _frame_seq, _start_ts_us, _last_frame_monotonic
    _frame_history.clear()
    _id_counts.clear()
    _frame_seq = 0
    _start_ts_us = None
    _last_frame_monotonic = None
    _push_table_update()
    _push_chart_update()


def append_log(text: str) -> None:
    if text is None:
        return
    lines = str(text).splitlines() or ['']
    for line in lines:
        entry = line.rstrip('\r')
        _log_buffer.append(entry)
        if len(_log_buffer) > _MAX_LOG_LINES:
            # deque already caps length, but pop left to be explicit
            try:
                _log_buffer.popleft()
            except Exception:
                pass
        if _log_writer:
            try:
                _log_writer(entry)
            except Exception:
                continue


def clear_log() -> None:
    _log_buffer.clear()
    if _log_clearer:
        try:
            _log_clearer()
        except Exception:
            pass


def top_identifier_stats(limit: int = 12) -> List[tuple[str, int]]:
    common = _id_counts.most_common(limit)
    return [(f"0x{id_:08X}", count) for id_, count in common]


def _push_table_update() -> None:
    if not _table_updater:
        return
    rows: List[Dict[str, Any]] = []
    for frame in reversed(_frame_history):
        if _filter_text and not _matches_filter(frame):
            continue
        rows.append(
            {
                'seq': frame.seq,
                'timestamp': f"{frame.relative_ms:,.3f}",
                'id_hex': frame.id_hex,
                'id_dec': frame.identifier,
                'dlc': frame.dlc,
                'data': ' '.join(frame.data_hex_pairs) if frame.data_hex_pairs else '—',
                'flags': frame.flags_label,
                'message': frame.message_label,
                'signals': frame.decoded_label,
            }
        )
    try:
        _table_updater(rows)
    except Exception:
        pass


def _push_chart_update() -> None:
    if not _chart_updater:
        return
    stats = top_identifier_stats()
    labels = [label for label, _ in stats]
    values = [count for _, count in stats]
    try:
        _chart_updater(labels, values)
    except Exception:
        pass


def _matches_filter(frame: CanFrame) -> bool:
    if not _filter_text:
        return True
    haystack = [
        frame.id_hex.lower(),
        str(frame.identifier),
        ' '.join(frame.data_hex_pairs).lower(),
        frame.flags_label.lower(),
        frame.raw.lower(),
        frame.message_label.lower() if frame.message_name else '',
        frame.decoded_label.lower() if frame.decoded_signals else '',
    ]
    token = _filter_text
    return any(token in fragment for fragment in haystack)


def refresh_decoded_frames() -> None:
    """Re-apply DBC decoding to the stored frame history."""
    if not _frame_history:
        return
    for frame in list(_frame_history):
        decoded = dbc_decode(frame.identifier, frame.data_bytes, extended=frame.extended)
        message_name, decoded_signals = _extract_decoded(decoded)
        frame.message_name = message_name
        frame.decoded_signals = decoded_signals
    _push_table_update()


def refresh_dbc_listing() -> None:
    global _dbc_status_label, _dbc_container
    label = _dbc_status_label
    container = _dbc_container
    if label is None or container is None:
        return

    try:
        entries = list_dbcs()
    except Exception:
        entries = []

    try:
        container.clear()
    except Exception:
        _dbc_status_label = None
        _dbc_container = None
        return

    if not entries:
        try:
            label.set_text('No DBC files loaded')
        except Exception:
            pass
        return

    try:
        label.set_text(f'{len(entries)} file(s) loaded')
    except Exception:
        pass

    try:
        with container:
            for entry in entries:
                name = str(entry.get('name') or 'Unnamed')
                count = int(entry.get('messages') or 0)
                ui.label(f'{name} — {count} messages').classes('text-sm text-neutral-700 dark:text-neutral-200')
    except Exception:
        pass


def _extract_decoded(decoded: Any) -> Tuple[Optional[str], List[Tuple[str, str]]]:
    message_name: Optional[str] = None
    decoded_signals: List[Tuple[str, str]] = []
    if not isinstance(decoded, dict):
        return message_name, decoded_signals

    raw_name = decoded.get('message_name')
    if isinstance(raw_name, str) and raw_name.strip():
        message_name = raw_name.strip()

    raw_signals = decoded.get('signals')
    if isinstance(raw_signals, list):
        for entry in raw_signals:
            if not isinstance(entry, (list, tuple)) or len(entry) != 2:
                continue
            name, value = entry
            decoded_signals.append((str(name), str(value)))

    return message_name, decoded_signals


__all__ = [
    'append_can_frame',
    'append_log',
    'clear_frames',
    'clear_log',
    'dark_mode_enabled',
    'register_chart_updater',
    'register_connection_indicator',
    'register_dbc_listing',
    'register_dark_mode_controller',
    'register_log',
    'register_table_updater',
    'seconds_since_last_frame',
    'set_connection_state',
    'set_dark_mode',
    'set_filter',
    'toggle_dark_mode',
    'top_identifier_stats',
    'refresh_decoded_frames',
    'refresh_dbc_listing',
]
