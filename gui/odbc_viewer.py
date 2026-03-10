# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Wavenumber LLC
"""
odbc_monkey GUI Viewer

A DearPyGui application to browse data from the odbc_monkey ODBC driver.
"""

from __future__ import annotations

import logging
import re
from pathlib import Path
from typing import Optional

import dearpygui.dearpygui as dpg
import pyodbc

log = logging.getLogger(__name__)

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_JSON_PATH = REPO_ROOT / "example" / "json"
DEFAULT_CONNECTION_STRING = f"DRIVER={{odbc-monkey}};DataSource={DEFAULT_JSON_PATH}"
DEFAULT_QUERY = "SELECT * FROM Parts"
MAX_DISPLAY_ROWS = 1000
WIDTH_SAMPLE_ROWS = 50
COLUMN_MIN_WIDTH = 96
COLUMN_MAX_WIDTH = 420
CELL_PREVIEW_LIMIT = 120
CHAR_WIDTH_PX = 8


class OdbcViewer:
    def __init__(self):
        self.connection: Optional[pyodbc.Connection] = None
        self.tables: list[str] = []
        self.current_data: list[list[object]] = []
        self.current_columns: list[str] = []

    def reset_results(self):
        self.current_data = []
        self.current_columns = []

    def connect(self, connection_string: str) -> tuple[bool, str]:
        """Connect to the ODBC data source."""
        try:
            if self.connection:
                self.connection.close()
            self.connection = pyodbc.connect(connection_string)
            self.reset_results()
            return True, "Connected successfully"
        except Exception as exc:
            return False, str(exc)

    def disconnect(self):
        """Disconnect from the data source."""
        if self.connection:
            self.connection.close()
            self.connection = None
        self.tables = []
        self.reset_results()

    def get_tables(self) -> list[str]:
        """Get a sorted list of unique tables from the data source."""
        if not self.connection:
            return []

        try:
            cursor = self.connection.cursor()
            table_names = sorted({row.table_name for row in cursor.tables()}, key=str.casefold)
            cursor.close()
            self.tables = table_names
            return self.tables
        except Exception as exc:
            log.error("Error getting tables: %s", exc)
            return []

    def execute_query(self, query: str) -> tuple[bool, str]:
        """Execute a SQL query and store results for display."""
        if not self.connection:
            return False, "Not connected"

        try:
            cursor = self.connection.cursor()
            cursor.execute(query)
            self.reset_results()

            if cursor.description:
                self.current_columns = [desc[0] for desc in cursor.description]
                self.current_data = [list(row) for row in cursor.fetchall()]
                message = f"Returned {len(self.current_data)} rows across {len(self.current_columns)} columns"
            else:
                message = "Query executed"

            cursor.close()
            return True, message
        except Exception as exc:
            return False, str(exc)


viewer = OdbcViewer()


def quote_identifier(identifier: str) -> str:
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", identifier):
        return identifier
    return f"[{identifier.replace(']', ']]')}]"


def format_cell_value(value: object) -> str:
    return "(null)" if value is None else str(value)


def truncate_text(text: str, limit: int = CELL_PREVIEW_LIMIT) -> str:
    if len(text) <= limit:
        return text
    return text[: limit - 3] + "..."


def estimate_column_width(column_name: str, rows: list[list[object]], column_index: int) -> int:
    max_chars = len(column_name)
    for row in rows[:WIDTH_SAMPLE_ROWS]:
        if column_index < len(row):
            max_chars = max(max_chars, len(format_cell_value(row[column_index])))

    estimated = (min(max_chars, 48) * CHAR_WIDTH_PX) + 24
    return max(COLUMN_MIN_WIDTH, min(COLUMN_MAX_WIDTH, estimated))


def update_status(message: str, is_error: bool = False):
    dpg.set_value("status_text", message)
    if is_error:
        dpg.configure_item("status_text", color=(255, 110, 110))
    else:
        dpg.configure_item("status_text", color=(110, 220, 140))


def update_results_summary(message: str):
    dpg.set_value("results_summary_text", message)


def set_selected_table(table_name: Optional[str] = None):
    if table_name:
        dpg.set_value("selected_table_text", f"Selected: {table_name}")
        dpg.set_value("tables_list", table_name)
    else:
        dpg.set_value("selected_table_text", "Selected: none")
        dpg.set_value("tables_list", "")


def refresh_tables():
    tables = viewer.get_tables() if viewer.connection else []
    dpg.set_value("table_count_text", f"{len(tables)} table(s)")
    apply_table_filter()


def apply_table_filter():
    filter_text = dpg.get_value("table_filter_input").strip().casefold()
    if filter_text:
        filtered = [table for table in viewer.tables if filter_text in table.casefold()]
    else:
        filtered = viewer.tables
    dpg.configure_item("tables_list", items=filtered)
    current_selection = dpg.get_value("tables_list")
    if current_selection not in filtered:
        set_selected_table(None)


def clear_data_table(message: str = "Run a query to view results"):
    if dpg.does_item_exist("data_table"):
        dpg.delete_item("data_table")

    with dpg.table(
        tag="data_table",
        parent="data_table_container",
        header_row=False,
        resizable=True,
        policy=dpg.mvTable_SizingFixedFit,
        borders_outerH=True,
        borders_innerV=True,
        borders_innerH=True,
        borders_outerV=True,
        scrollX=True,
        scrollY=True,
    ):
        dpg.add_table_column(label="Result", width_fixed=True, init_width_or_weight=600)
        with dpg.table_row():
            dpg.add_text(message)

    update_results_summary("No results")


def update_data_table():
    if dpg.does_item_exist("data_table"):
        dpg.delete_item("data_table")

    rows_to_show = viewer.current_data[:MAX_DISPLAY_ROWS]

    with dpg.table(
        tag="data_table",
        parent="data_table_container",
        header_row=True,
        resizable=True,
        policy=dpg.mvTable_SizingFixedFit,
        borders_outerH=True,
        borders_innerV=True,
        borders_innerH=True,
        borders_outerV=True,
        scrollX=True,
        scrollY=True,
        row_background=True,
        freeze_rows=1,
        clipper=True,
    ):
        for index, column_name in enumerate(viewer.current_columns):
            dpg.add_table_column(
                label=column_name,
                width_fixed=True,
                init_width_or_weight=estimate_column_width(column_name, rows_to_show, index),
            )

        for row in rows_to_show:
            with dpg.table_row():
                for value in row:
                    full_text = format_cell_value(value)
                    display_text = truncate_text(full_text)
                    text_id = dpg.add_text(display_text)
                    if display_text != full_text:
                        with dpg.tooltip(parent=text_id):
                            dpg.add_text(full_text, wrap=700)

    summary = f"{len(viewer.current_data)} row(s), {len(viewer.current_columns)} column(s)"
    if len(viewer.current_data) > MAX_DISPLAY_ROWS:
        summary += f" - showing first {MAX_DISPLAY_ROWS}"
    update_results_summary(summary)


def set_query(query: str):
    dpg.set_value("query_input", query)


def clear_query():
    set_query("")


def on_connect():
    connection_string = dpg.get_value("connection_string").strip()
    if not connection_string:
        update_status("Enter a connection string first", is_error=True)
        return

    update_status("Connecting...")
    success, message = viewer.connect(connection_string)
    if not success:
        update_status(f"Connection failed: {message}", is_error=True)
        return

    refresh_tables()
    dpg.configure_item("disconnect_btn", enabled=True)
    dpg.configure_item("connect_btn", enabled=False)
    dpg.configure_item("execute_btn", enabled=True)
    dpg.configure_item("refresh_tables_btn", enabled=True)
    update_status(message)
    clear_data_table()


def on_disconnect():
    viewer.disconnect()
    dpg.configure_item("tables_list", items=[])
    dpg.set_value("table_filter_input", "")
    dpg.set_value("table_count_text", "0 table(s)")
    set_selected_table(None)
    dpg.configure_item("disconnect_btn", enabled=False)
    dpg.configure_item("connect_btn", enabled=True)
    dpg.configure_item("execute_btn", enabled=False)
    dpg.configure_item("refresh_tables_btn", enabled=False)
    clear_data_table("Disconnected")
    update_status("Disconnected")


def on_refresh_tables():
    if not viewer.connection:
        update_status("Connect first", is_error=True)
        return
    refresh_tables()
    update_status(f"Refreshed table list - {len(viewer.tables)} table(s)")


def on_table_filter(_sender, _app_data):
    apply_table_filter()


def on_table_select(_sender, app_data):
    if not app_data:
        return

    table_name = str(app_data)
    set_selected_table(table_name)
    set_query(f"SELECT * FROM {quote_identifier(table_name)}")
    update_status(f"Loaded query for {table_name}")


def on_execute_query():
    query = dpg.get_value("query_input").strip()
    if not query:
        update_status("Please enter a query", is_error=True)
        return

    update_status("Executing query...")
    success, message = viewer.execute_query(query)
    if not success:
        clear_data_table("Query failed")
        update_status(f"Query failed: {message}", is_error=True)
        return

    if viewer.current_columns:
        update_data_table()
    else:
        clear_data_table("Query returned no tabular result")

    update_status(message)


def main():
    dpg.create_context()

    with dpg.window(tag="main_window"):
        with dpg.collapsing_header(label="Connection", default_open=True):
            dpg.add_input_text(
                tag="connection_string",
                default_value=DEFAULT_CONNECTION_STRING,
                width=-1,
            )
            dpg.add_text(
                default_value="Format: DRIVER={odbc-monkey};DataSource=C:\\path\\to\\json",
                color=(160, 160, 160),
            )

            with dpg.group(horizontal=True):
                dpg.add_button(tag="connect_btn", label="Connect", callback=on_connect, width=110)
                dpg.add_button(
                    tag="disconnect_btn",
                    label="Disconnect",
                    callback=on_disconnect,
                    width=110,
                    enabled=False,
                )

        dpg.add_spacer(height=8)

        with dpg.group(horizontal=True):
            with dpg.child_window(width=280, height=-52):
                dpg.add_text("Tables")
                with dpg.group(horizontal=True):
                    dpg.add_input_text(
                        tag="table_filter_input",
                        hint="Filter tables",
                        width=170,
                        callback=on_table_filter,
                    )
                    dpg.add_button(
                        tag="refresh_tables_btn",
                        label="Refresh",
                        callback=on_refresh_tables,
                        width=80,
                        enabled=False,
                    )
                dpg.add_text(tag="table_count_text", default_value="0 table(s)", color=(160, 160, 160))
                dpg.add_text(tag="selected_table_text", default_value="Selected: none", color=(160, 160, 160))
                dpg.add_listbox(
                    tag="tables_list",
                    items=[],
                    num_items=22,
                    callback=on_table_select,
                    width=-1,
                )

            dpg.add_spacer(width=10)

            with dpg.child_window(width=-1, height=-52):
                dpg.add_text("SQL Query")
                with dpg.group(horizontal=True):
                    dpg.add_button(label="Clear", callback=clear_query, width=80)
                    dpg.add_button(
                        tag="execute_btn",
                        label="Execute",
                        callback=on_execute_query,
                        width=100,
                        enabled=False,
                    )

                dpg.add_input_text(
                    tag="query_input",
                    default_value=DEFAULT_QUERY,
                    width=-1,
                    height=72,
                    multiline=True,
                    tab_input=True,
                )

                dpg.add_spacer(height=10)
                with dpg.group(horizontal=True):
                    dpg.add_text("Results")
                    dpg.add_spacer(width=10)
                    dpg.add_text(tag="results_summary_text", default_value="No results", color=(160, 160, 160))

                with dpg.child_window(tag="data_table_container", height=-1, horizontal_scrollbar=True):
                    clear_data_table()

        with dpg.group(horizontal=True):
            dpg.add_text("Status:", color=(150, 150, 150))
            dpg.add_text(tag="status_text", default_value="Not connected", color=(200, 200, 200))

    dpg.create_viewport(title="odbc_monkey Data Viewer", width=1320, height=860)
    dpg.setup_dearpygui()
    dpg.set_primary_window("main_window", True)
    dpg.show_viewport()
    dpg.start_dearpygui()
    dpg.destroy_context()


if __name__ == "__main__":
    main()
