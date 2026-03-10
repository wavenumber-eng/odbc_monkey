# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Wavenumber LLC
"""
UDP Log Viewer for odbc-monkey ODBC Driver
Listens on UDP port for log messages from the driver.

Usage:
    python udp_log_viewer.py [port]

Default port is 9999. Set LogPort in connection string to match:
    DRIVER={odbc-monkey};DataSource=C:\\path;LogPort=9999
"""

import socket
import sys
import argparse
from datetime import datetime

# Default configuration
DEFAULT_PORT = 9999

# ANSI colors for terminal
COLORS = {
    'reset': '\033[0m',
    'red': '\033[91m',
    'green': '\033[92m',
    'yellow': '\033[93m',
    'blue': '\033[94m',
    'magenta': '\033[95m',
    'cyan': '\033[96m',
    'gray': '\033[90m',
}


def colorize(text: str, color: str) -> str:
    """Add ANSI color to text."""
    return f"{COLORS.get(color, '')}{text}{COLORS['reset']}"


def get_log_color(message: str) -> str:
    """Determine color based on log content."""
    msg_lower = message.lower()
    if 'error' in msg_lower or 'fail' in msg_lower:
        return 'red'
    elif 'warn' in msg_lower:
        return 'yellow'
    elif 'sql' in msg_lower or 'query' in msg_lower:
        return 'cyan'
    elif 'connect' in msg_lower:
        return 'green'
    elif 'disconnect' in msg_lower or 'free' in msg_lower:
        return 'magenta'
    return 'reset'


def main():
    parser = argparse.ArgumentParser(description='UDP Log Viewer for odbc-monkey ODBC Driver')
    parser.add_argument('port', nargs='?', type=int, default=DEFAULT_PORT,
                        help=f'UDP port to listen on (default: {DEFAULT_PORT})')
    args = parser.parse_args()

    port = args.port

    print(colorize(f"=== odbc-monkey UDP Log Viewer ===", 'cyan'))
    print(colorize(f"Listening on UDP port {port}...", 'gray'))
    print(colorize(f"Press Ctrl+C to exit", 'gray'))
    print()

    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        sock.bind(('127.0.0.1', port))
    except OSError as e:
        print(colorize(f"Error: Could not bind to port {port}: {e}", 'red'))
        print(colorize("Is another instance already running?", 'yellow'))
        sys.exit(1)

    message_count = 0

    try:
        while True:
            data, addr = sock.recvfrom(4096)
            message_count += 1

            try:
                # Decode as UTF-8, strip whitespace
                message = data.decode('utf-8').strip()
            except UnicodeDecodeError:
                # Fallback to latin-1 if UTF-8 fails
                message = data.decode('latin-1').strip()

            # Timestamp
            timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]

            # Colorize based on content
            color = get_log_color(message)

            # Print formatted log line
            print(f"{colorize(timestamp, 'gray')} {colorize(message, color)}")

    except KeyboardInterrupt:
        print()
        print(colorize(f"Received {message_count} messages", 'cyan'))
        print(colorize("Goodbye!", 'green'))
    finally:
        sock.close()


if __name__ == "__main__":
    main()
