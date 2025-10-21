from nicegui import ui

from gui import app


def main() -> None:
    """Entrypoint for the Bolt CAN dashboard."""
    app.init()
    ui.run(
        title='Bolt CAN Monitor',
        reload=False,
        show=True,
        host='127.0.0.1',
        port=8075,
    )


if __name__ == '__main__':
    main()
