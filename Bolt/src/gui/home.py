"""Home page layout for the Bolt CAN dashboard."""

from __future__ import annotations

from typing import Any, Dict, List

from nicegui import events, ui

from dbc import clear_dbcs, list_dbcs, load_dbc
from gui import state as st
from gui.components import make_can_table, make_identifier_chart, make_text_console
from usb_serial import serial_handler


def build_home() -> None:
    dark_controller = ui.dark_mode()
    st.register_dark_mode_controller(dark_controller)

    with ui.header().classes('bg-primary text-white px-4 py-2 justify-between items-center dark:bg-slate-900'):
        ui.label('Bolt CAN Monitor').classes('text-lg font-semibold tracking-wide')
        with ui.row().classes('items-center gap-4'):
            status_label = ui.label('Disconnected').classes('text-sm')
            st.register_connection_indicator(status_label)

            def _on_dark_mode_change(e: events.ValueChangeEventArguments) -> None:
                st.set_dark_mode(bool(e.value))

            dark_toggle = ui.switch('Dark mode', value=st.dark_mode_enabled()).props('dense color=primary')
            dark_toggle.on('update:model-value', _on_dark_mode_change)

    with ui.column().classes('w-full max-w-full gap-4 px-4 pb-6 dark:bg-slate-950 dark:text-gray-100').style('margin-top: 12px;'):
        _build_connection_card()
        _build_dbc_section()
        _build_filters_and_actions()
        _build_data_section()
        _build_log_section()


def _build_connection_card() -> None:
    with ui.card().classes('w-full max-w-full dark:bg-slate-900 dark:text-gray-100'):
        ui.label('Serial Connection').classes('text-md font-medium')
        ui.separator()
        with ui.row().classes('w-full items-end gap-3 flex-wrap'):
            port_select = ui.select(
                options=[],
                label='Port',
                with_input=True,
            ).props('emit-value map-options').classes('min-w-[200px]')
            baud_input = ui.number(label='Baud rate', value=921600, format='%.0f').props('step=1200')
            message_label = ui.label('').classes('text-sm text-neutral-500 dark:text-neutral-300 grow')

            async def refresh_ports(_: Any = None) -> None:
                ports = serial_handler.list_ports()
                options: List[Dict[str, Any]] = [
                    {'value': dev, 'label': f'{dev} — {desc}'}
                    for dev, desc in ports
                ]
                if not options:
                    message_label.set_text('No serial ports found')
                else:
                    message_label.set_text(f'Found {len(options)} port(s)')
                port_select.options = options
                if options and not port_select.value:
                    port_select.value = options[0]['value']
                await port_select.update()

            async def connect(_: Any = None) -> None:
                port = port_select.value
                baud = int(baud_input.value or 0)
                if not port:
                    ui.notify('Select a port before connecting', color='negative')
                    return
                ok, msg = serial_handler.connect(port, baud if baud > 0 else 921600)
                ui.notify(msg, color='positive' if ok else 'negative')
                st.append_log(msg)
                if ok:
                    st.set_connection_state(True, msg)

            async def disconnect(_: Any = None) -> None:
                ok, msg = serial_handler.disconnect()
                ui.notify(msg, color='positive' if ok else 'warning')
                st.append_log(msg)
                if ok:
                    st.set_connection_state(False, msg)

            ui.button('Refresh', on_click=refresh_ports).props('outline')
            ui.button('Connect', on_click=connect).props('color=primary')
            ui.button('Disconnect', on_click=disconnect).props('color=secondary outline')

            # Populate ports on load
            ui.timer(0.1, refresh_ports, once=True)


def _build_dbc_section() -> None:
    with ui.card().classes('w-full max-w-full dark:bg-slate-900 dark:text-gray-100'):
        ui.label('DBC Decoder').classes('text-md font-medium')
        ui.separator()

        status_label = ui.label('No DBC files loaded').classes('text-sm text-neutral-500 dark:text-neutral-300')
        file_container = ui.column().classes('w-full gap-1')

        def refresh_listing() -> None:
            file_container.clear()
            entries = list_dbcs()
            if not entries:
                status_label.set_text('No DBC files loaded')
                return
            status_label.set_text(f'{len(entries)} file(s) loaded')
            with file_container:
                for entry in entries:
                    name = str(entry.get('name') or 'Unnamed')
                    count = int(entry.get('messages') or 0)
                    ui.label(f'{name} — {count} messages').classes('text-sm text-neutral-700 dark:text-neutral-200')

        def handle_clear(_: Any = None) -> None:
            clear_dbcs()
            st.append_log('Cleared loaded DBC files')
            ui.notify('Cleared all DBC files', color='warning')
            st.refresh_decoded_frames()
            refresh_listing()

        async def handle_upload(event: events.UploadEventArguments) -> None:
            data = event.content.read()
            event.content.seek(0)
            payload = data or b''
            ok, message = load_dbc(event.name or 'DBC file', payload)
            st.append_log(message)
            ui.notify(message, color='positive' if ok else 'negative')
            st.refresh_decoded_frames()
            refresh_listing()

        with ui.row().classes('w-full items-center gap-3 flex-wrap mt-2'):
            upload = ui.upload(label='Add DBC', on_upload=handle_upload, multiple=True, auto_upload=True)
            upload.props('accept=.dbc,.DBC').classes('max-w-full')
            ui.button('Clear DBCs', on_click=handle_clear).props('flat color=warning')

        refresh_listing()


def _build_filters_and_actions() -> None:
    with ui.row().classes('w-full items-center gap-3 flex-wrap dark:text-gray-100'):
        filter_input = ui.input('Filter by ID, payload, or text').classes('min-w-[220px]')

        def _on_filter(e: events.ValueChangeEventArguments) -> None:
            st.set_filter(e.value or '')

        filter_input.on('update:model-value', _on_filter)

        ui.button('Clear Frames', on_click=st.clear_frames).props('flat color=warning')
        ui.button('Clear Log', on_click=st.clear_log).props('flat color=warning')


def _build_data_section() -> None:
    with ui.row().classes('w-full items-stretch gap-4 flex-wrap'):
        with ui.column().classes('grow min-w-[340px] gap-2'):
            table, update_table = make_can_table()
            st.register_table_updater(update_table)
        with ui.column().classes('basis-[360px] grow gap-2'):
            chart, update_chart = make_identifier_chart()
            st.register_chart_updater(update_chart)


def _build_log_section() -> None:
    _, write, clear = make_text_console('Monitor Log')
    st.register_log(write, clear)
