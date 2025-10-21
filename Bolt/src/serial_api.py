from typing import Any, Dict
from nicegui import app
from fastapi import Body
from usb_serial.serial_handler import list_ports, connect, is_connected, selected_port


def _ports_endpoint() -> Dict[str, Any]:
    ports = [
        {"device": dev, "description": desc}
        for dev, desc in list_ports()
    ]
    return {"ports": ports, "connected": is_connected(), "selected": selected_port()}


def _connect_endpoint(payload: Dict[str, Any] = Body(...)) -> Dict[str, Any]:
    port = str(payload.get("port", "")).strip()
    baud = int(payload.get("baud", 115200))
    if not port:
        return {"ok": False, "message": "Missing 'port'"}
    ok, msg = connect(port, baud)
    return {"ok": ok, "message": msg, "connected": is_connected(), "selected": selected_port()}


def register():
    # Avoid duplicate registration if reloads occur
    try:
        app.add_api_route('/api/serial/ports', _ports_endpoint, methods=['GET'])
    except Exception:
        pass
    try:
        app.add_api_route('/api/serial/connect', _connect_endpoint, methods=['POST'])
    except Exception:
        pass

