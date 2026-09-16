# Copyright (C) 2026 Joseph Crowell
# SPDX-License-Identifier: GPL-2.0-or-later

"""Run the real frontend without sibling QML files or development import paths."""

import argparse
import ctypes
import http.server
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import uuid


def x11_window_has_icon(title, process_id, timeout):
    """Return whether the mapped X11 window publishes a nonempty _NET_WM_ICON."""
    if not os.environ.get("DISPLAY"):
        return None
    try:
        x11 = ctypes.CDLL("libX11.so.6")
    except OSError:
        return None
    display_type = ctypes.c_void_p
    window_type = ctypes.c_ulong
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = display_type
    x11.XDefaultRootWindow.argtypes = [display_type]
    x11.XDefaultRootWindow.restype = window_type
    x11.XQueryTree.argtypes = [
        display_type, window_type, ctypes.POINTER(window_type),
        ctypes.POINTER(window_type), ctypes.POINTER(ctypes.POINTER(window_type)),
        ctypes.POINTER(ctypes.c_uint)]
    x11.XFetchName.argtypes = [display_type, window_type,
                               ctypes.POINTER(ctypes.c_void_p)]
    x11.XInternAtom.argtypes = [display_type, ctypes.c_char_p, ctypes.c_int]
    x11.XInternAtom.restype = ctypes.c_ulong
    x11.XGetWindowProperty.argtypes = [
        display_type, window_type, ctypes.c_ulong, ctypes.c_long, ctypes.c_long,
        ctypes.c_int, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong),
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_ulong),
        ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_void_p)]
    x11.XFree.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [display_type]

    display = x11.XOpenDisplay(None)
    if not display:
        return None
    try:
        icon_atom = x11.XInternAtom(display, b"_NET_WM_ICON", False)
        pid_atom = x11.XInternAtom(display, b"_NET_WM_PID", False)
        root = x11.XDefaultRootWindow(display)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            queue = [root]
            found_title = False
            while queue:
                parent_window = queue.pop()
                returned_root = window_type()
                returned_parent = window_type()
                children = ctypes.POINTER(window_type)()
                child_count = ctypes.c_uint()
                if not x11.XQueryTree(display, parent_window,
                                      ctypes.byref(returned_root),
                                      ctypes.byref(returned_parent),
                                      ctypes.byref(children),
                                      ctypes.byref(child_count)):
                    continue
                try:
                    child_windows = [children[index]
                                     for index in range(child_count.value)]
                    queue.extend(child_windows)
                    for window in child_windows:
                        matches_process = False
                        pid_type = ctypes.c_ulong()
                        pid_format = ctypes.c_int()
                        pid_count = ctypes.c_ulong()
                        pid_remaining = ctypes.c_ulong()
                        pid_pointer = ctypes.c_void_p()
                        pid_status = x11.XGetWindowProperty(
                            display, window, pid_atom, 0, 1, False, 0,
                            ctypes.byref(pid_type), ctypes.byref(pid_format),
                            ctypes.byref(pid_count), ctypes.byref(pid_remaining),
                            ctypes.byref(pid_pointer))
                        try:
                            if (pid_status == 0 and pid_pointer and pid_count.value and
                                    pid_format.value == 32):
                                pid_values = ctypes.cast(
                                    pid_pointer, ctypes.POINTER(ctypes.c_ulong))
                                matches_process = pid_values[0] == process_id
                        finally:
                            if pid_pointer:
                                x11.XFree(pid_pointer)
                        if not matches_process:
                            continue
                        found_title = True
                        actual_type = ctypes.c_ulong()
                        actual_format = ctypes.c_int()
                        item_count = ctypes.c_ulong()
                        remaining = ctypes.c_ulong()
                        property_pointer = ctypes.c_void_p()
                        status = x11.XGetWindowProperty(
                            display, window, icon_atom, 0, 1024 * 1024, False, 0,
                            ctypes.byref(actual_type), ctypes.byref(actual_format),
                            ctypes.byref(item_count), ctypes.byref(remaining),
                            ctypes.byref(property_pointer))
                        try:
                            if (status != 0 or not property_pointer or
                                    item_count.value < 3):
                                continue
                            values = ctypes.cast(
                                property_pointer, ctypes.POINTER(ctypes.c_ulong))
                            if (actual_format.value == 32 and values[0] > 0 and
                                    values[1] > 0 and
                                    item_count.value >= 2 + values[0] * values[1]):
                                return True
                        finally:
                            if property_pointer:
                                x11.XFree(property_pointer)
                finally:
                    if children:
                        x11.XFree(children)
            if found_title:
                return False
            time.sleep(0.05)
        return False
    finally:
        x11.XCloseDisplay(display)


def check_deployed_application(executable, timeout):
    ready = threading.Event()
    token = uuid.uuid4().hex
    html = ("<!doctype html><html><head><title>Deployment fixture</title></head>"
            "<body><h1>Embedded QML deployment test</h1><script>"
            "window.addEventListener('load', () => fetch('/ready', "
            "{method: 'POST', body: '" + token + "'}));"
            "</script></body></html>").encode("utf-8")

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path != "/":
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(html)))
            self.end_headers()
            self.wfile.write(html)

        def do_POST(self):
            body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
            if self.path != "/ready" or body != token.encode("ascii"):
                self.send_error(400)
                return
            self.send_response(204)
            self.end_headers()
            ready.set()

        def log_message(self, *_args):
            pass

    with tempfile.TemporaryDirectory(prefix="webappcontainer-deployed-") as temporary:
        root = Path(temporary)
        relocated = root / executable.name
        # Intentionally do not copy WebAppContainer/, qmldir, QML or qt.conf.
        shutil.copy2(executable, relocated)
        icon = root / "window-icon.svg"
        icon.write_text(
            '<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64">'
            '<rect width="64" height="64" fill="#1a73e8"/></svg>',
            encoding="utf-8")
        environment = os.environ.copy()
        for name in ("QML_IMPORT_PATH", "QML2_IMPORT_PATH", "QML_DISK_CACHE_PATH"):
            environment.pop(name, None)
        environment["QML_DISABLE_DISK_CACHE"] = "1"
        for name, directory in (("XDG_DATA_HOME", "data"),
                                ("XDG_CACHE_HOME", "cache"),
                                ("XDG_CONFIG_HOME", "config")):
            path = root / directory
            path.mkdir()
            environment[name] = str(path)

        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        process = None
        failure = None
        log_path = root / "application.log"
        try:
            with log_path.open("wb") as output:
                process = subprocess.Popen(
                    [str(relocated), "--url", f"http://127.0.0.1:{server.server_port}/",
                     "--name", "QML deployment test", "--profile", "deployment-test",
                     "--icon", str(icon), "--tray-icon", str(icon),
                     "--no-notify"],
                    cwd=root, env=environment, stdout=output, stderr=subprocess.STDOUT)
                deadline = time.monotonic() + timeout
                while not ready.wait(0.1):
                    if process.poll() is not None:
                        failure = f"Relocated application exited with {process.returncode}"
                        break
                    if time.monotonic() >= deadline:
                        failure = "Application did not load the fixture and execute JavaScript"
                        break
                if failure is None and process.poll() is not None:
                    failure = f"Application exited after page load with {process.returncode}"
        finally:
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)

        log = log_path.read_text(encoding="utf-8", errors="replace")
        if "QQmlApplicationEngine failed to load component" in log:
            failure = "Production QML root failed to load after relocation"
        if "Could not load application icon" in log or "Could not load tray icon" in log:
            failure = "Relocated application did not accept the supplied SVG icon"
        if failure:
            print(failure, file=sys.stderr)
            print(log, file=sys.stderr)
            return 1
        print("Relocated executable loaded the fixture and executed JavaScript "
              "without build-tree QML files or import-path overrides.")
        return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--timeout", type=float, default=30)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    sys.exit(check_deployed_application(args.executable.resolve(strict=True), args.timeout))
