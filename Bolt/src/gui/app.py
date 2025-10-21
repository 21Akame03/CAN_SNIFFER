"""Application bootstrap for the Bolt CAN dashboard."""

from __future__ import annotations

from typing import List

from nicegui import run as ng_run
from nicegui import ui

from gui import state as st
from gui.home import build_home
from jtag.data_processor import process_line
from usb_serial.serial_handler import get_pending_lines, is_connected, selected_port


def init() -> None:
    """Initialise NiceGUI, layout, and background workers."""
    # Avoid starting a process pool in restricted environments (NiceGUI quirk)
    ng_run.setup = lambda: None  # type: ignore

    def _drain_serial(_: float | None = None) -> None:
        try:
            lines: List[str] = get_pending_lines(300)
        except Exception as exc:
            st.append_log(f'[Reader] Failed to poll serial: {exc}')
            return
        if not lines:
            return
        for line in lines:
            process_line(line)

    def _refresh_status(_: float | None = None) -> None:
        connected = is_connected()
        port = selected_port() or '—'
        idle = st.seconds_since_last_frame()
        if connected:
            if idle is None:
                message = f'Connected ({port})'
            else:
                message = f'Connected ({port}) · last frame {idle:.1f}s ago'
            st.set_connection_state(True, message)
        else:
            st.set_connection_state(False, 'Disconnected')

    @ui.page('/')
    def _home_page() -> None:
        build_home()
        # Attach timers to the page to avoid global UI elements alongside ui.page usage
        ui.timer(0.1, _drain_serial, active=True)
        ui.timer(0.5, _refresh_status, active=True)
