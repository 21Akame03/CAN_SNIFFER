import serial
import platform
import serial.tools.list_ports
import threading
import queue
import time
from typing import List, Optional, Tuple

# Simple serial connection manager for listing and connecting to COM ports.

_SER: Optional[serial.Serial] = None
_SELECTED_PORT: Optional[str] = None
_READ_THREAD: Optional[threading.Thread] = None
_STOP_EVENT: Optional[threading.Event] = None
_READ_Q: "queue.Queue[str]" = queue.Queue()


def list_ports() -> List[Tuple[str, str]]:
    """Return a list of available serial ports as (device, description)."""
    ports = serial.tools.list_ports.comports()
    return [(p.device, p.description or p.device) for p in ports]


def connect(
    port: str, baudrate: int = 115200, timeout: float = 1.0
) -> Tuple[bool, str]:
    """Attempt to open the given serial port.

    Returns (ok, message). On success keeps a global handle for later use.
    """
    global _SER, _SELECTED_PORT
    # Close any previous connection
    try:
        if _SER and _SER.is_open:
            _SER.close()
    except Exception:
        pass
    _SER = None

    try:
        s = serial.Serial(port=port, baudrate=baudrate, timeout=timeout)
        if not s.is_open:
            s.open()
        _SER = s
        _SELECTED_PORT = port
        # Start reader thread now that we are connected
        try:
            start_reader()
        except Exception:
            pass
        try:
            push_jtag_line(f"[Serial] Connected: {port} @ {baudrate}")
        except Exception:
            pass
        return True, f"Connected to {port}"
    except Exception as e:
        _SELECTED_PORT = None
        return False, f"Failed to connect: {e}"


def is_connected() -> bool:
    try:
        return bool(_SER and _SER.is_open)
    except Exception:
        return False


def selected_port() -> Optional[str]:
    return _SELECTED_PORT


def disconnect() -> Tuple[bool, str]:
    """Close the serial port and stop reader."""
    global _SER, _SELECTED_PORT
    try:
        stop_reader()
        if _SER and _SER.is_open:
            _SER.close()
        _SER = None
        _SELECTED_PORT = None
        return True, "Disconnected"
    except Exception as e:
        return False, f"Failed to disconnect: {e}"


# Backwards compatibility helpers (kept for reference)
def check_os() -> int:
    sys = platform.system()
    if sys == "Darwin":
        return 1
    if sys == "Linux":
        return 2
    if sys == "Windows":
        return 3
    return 0


def find_port_name(os_version: int) -> list:
    # Historically returned just device names; keep similar behavior.
    return [p for p, _ in list_ports()]


def read_JTAG():
    """Deprecated shim; reader is started automatically on connect()."""
    start_reader()


def _reader_loop():
    global _SER, _STOP_EVENT, _READ_Q
    buf = bytearray()
    last_activity = time.monotonic()

    def _emit_line(raw: bytes) -> None:
        try:
            _READ_Q.put(raw.decode(errors="replace"))
        except Exception:
            _READ_Q.put("<binary>")

    while _STOP_EVENT and not _STOP_EVENT.is_set():
        try:
            if not (_SER and _SER.is_open):
                time.sleep(0.2)
                continue
            # Read available bytes (timeout driven)
            chunk = _SER.read(256)
            if not chunk:
                # If we have buffered data but no newline, flush occasionally
                if buf and (time.monotonic() - last_activity) > 0.5:
                    _emit_line(bytes(buf))
                    buf.clear()
                continue

            buf.extend(chunk)
            last_activity = time.monotonic()

            # Split on either LF or CR, whichever comes first
            cursor = 0
            upper = len(buf)
            while cursor < upper:
                b = buf[cursor]
                if b not in (0x0A, 0x0D):
                    cursor += 1
                    continue

                line = buf[:cursor]
                # Consume newline sequence (handle CRLF gracefully)
                drop = 1
                if b == 0x0D and cursor + 1 < upper and buf[cursor + 1] == 0x0A:
                    drop = 2
                else:
                    # Remove stray carriage return that may precede LF
                    if line.endswith(b"\r"):
                        line = line[:-1]

                _emit_line(bytes(line))

                # Remove processed bytes + newline terminator
                del buf[:cursor + drop]
                upper = len(buf)
                cursor = 0

            # Guard against extremely long buffers that never newline
            if len(buf) > 4096:
                _emit_line(bytes(buf))
                buf.clear()
        except Exception:
            # Be resilient: small backoff on errors
            time.sleep(0.1)
    # Flush remaining buffer on stop
    if buf:
        _emit_line(bytes(buf))


def start_reader():
    """Start background reader thread if not already running."""
    global _READ_THREAD, _STOP_EVENT
    if _READ_THREAD and _READ_THREAD.is_alive():
        return
    _STOP_EVENT = threading.Event()
    _READ_THREAD = threading.Thread(target=_reader_loop, name="serial-reader", daemon=True)
    _READ_THREAD.start()


def stop_reader():
    """Stop background reader thread (best-effort)."""
    global _READ_THREAD, _STOP_EVENT
    try:
        if _STOP_EVENT:
            _STOP_EVENT.set()
        if _READ_THREAD and _READ_THREAD.is_alive():
            _READ_THREAD.join(timeout=1.0)
    except Exception:
        pass
    finally:
        _READ_THREAD = None


def get_pending_lines(max_items: int = 200) -> List[str]:
    """Drain up to max_items lines read from the serial/JTAG stream."""
    out: List[str] = []
    for _ in range(max_items):
        try:
            out.append(_READ_Q.get_nowait())
        except Exception:
            break
    return out


def push_jtag_line(line: str) -> None:
    """Externally push a line into the JTAG output queue."""
    try:
        _READ_Q.put(str(line))
    except Exception:
        pass


if __name__ == "__main__":
    print(find_port_name(check_os()))
