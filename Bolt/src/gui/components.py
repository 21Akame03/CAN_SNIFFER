"""Reusable NiceGUI components for the Bolt dashboard."""

from __future__ import annotations

from typing import Callable, Dict, List, Tuple

from nicegui import ui


TableUpdater = Callable[[List[Dict[str, str]]], None]
ChartUpdater = Callable[[List[str], List[int]], None]


def make_can_table() -> Tuple[ui.table, TableUpdater]:
    columns = [
        {'name': 'timestamp', 'label': 'Time (ms)', 'field': 'timestamp', 'align': 'left', 'sortable': True},
        {'name': 'id_hex', 'label': 'ID (hex)', 'field': 'id_hex', 'align': 'left', 'sortable': True},
        {'name': 'id_dec', 'label': 'ID (dec)', 'field': 'id_dec', 'align': 'left', 'sortable': True},
        {'name': 'message', 'label': 'Message', 'field': 'message', 'align': 'left', 'sortable': True},
        {'name': 'dlc', 'label': 'DLC', 'field': 'dlc', 'align': 'right', 'sortable': True},
        {'name': 'data', 'label': 'Data', 'field': 'data', 'align': 'left'},
        {'name': 'signals', 'label': 'Decoded Signals', 'field': 'signals', 'align': 'left'},
        {'name': 'flags', 'label': 'Flags', 'field': 'flags', 'align': 'left'},
    ]

    table = ui.table(
        columns=columns,
        rows=[],
        row_key='seq',
        pagination={'rowsPerPage': 20, 'rowsNumber': 0},
    ).classes('w-full text-sm dark:bg-slate-900 dark:text-gray-100').props('dense flat wrap-cells')

    def update(rows: List[Dict[str, str]]) -> None:
        table.rows = rows
        table.update()

    return table, update


def make_identifier_chart() -> Tuple[ui.echart, ChartUpdater]:
    options: Dict[str, object] = {
        'title': {'text': 'Top CAN IDs', 'left': 'center', 'top': 10},
        'tooltip': {'trigger': 'axis'},
        'grid': {'left': 60, 'right': 20, 'top': 60, 'bottom': 60},
        'xAxis': {
            'type': 'category',
            'data': [],
            'axisLabel': {'rotate': 45},
        },
        'yAxis': {
            'type': 'value',
            'name': 'Frames',
        },
        'dataZoom': [
            {'type': 'slider', 'xAxisIndex': 0, 'bottom': 20},
            {'type': 'inside', 'xAxisIndex': 0},
        ],
        'series': [
            {
                'name': 'Frames',
                'type': 'bar',
                'data': [],
                'itemStyle': {'color': '#2563EB'},
            }
        ],
    }

    chart = ui.echart(options).classes('w-full dark:bg-slate-900 rounded-md').style('height: 320px')

    def update(labels: List[str], counts: List[int]) -> None:
        chart.options['xAxis']['data'] = labels
        chart.options['series'][0]['data'] = counts
        chart.update()

    return chart, update


def make_text_console(title: str) -> Tuple[ui.log, Callable[[str], None], Callable[[], None]]:
    with ui.card().classes('w-full dark:bg-slate-900 dark:text-gray-100') as card:
        ui.label(title).classes('text-md font-medium')
        log = ui.log(max_lines=400).classes(
            'w-full h-56 font-mono text-xs bg-white text-neutral-900 dark:bg-neutral-900 dark:text-green-300'
        )

    def write(text: str) -> None:
        if text is None:
            return
        lines = str(text).splitlines() or ['']
        for line in lines:
            log.push(line)

    def clear() -> None:
        log.clear()

    return log, write, clear


__all__ = ['make_can_table', 'make_identifier_chart', 'make_text_console']
