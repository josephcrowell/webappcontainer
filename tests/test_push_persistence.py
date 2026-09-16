# Copyright (C) 2026 Joseph Crowell
# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import http.server
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading


VAPID_KEY = "BNO4fIv439RpvbReeABNlDNiiBD2Maykn7EVnwsPseH7-P5hjnzZLEfnejXVP7Zt6MFoKqKeHm4nV9BHvbgoRPg"
WORKER = b"self.addEventListener('push', event => {});"


def page(mode):
    operation = (f"registration.pushManager.subscribe({{userVisibleOnly:true,"
                 f"applicationServerKey:'{VAPID_KEY}'}})" if mode == "create" else
                 "registration.pushManager.getSubscription()")
    return f"""<!doctype html><title>Running</title><script>
    (async () => {{
      try {{
        const registration = await navigator.serviceWorker.register('/worker.js');
        await navigator.serviceWorker.ready;
        const subscription = await {operation};
        await fetch('/result', {{method:'POST', body: subscription ? subscription.endpoint : 'MISSING'}});
      }} catch (error) {{
        await fetch('/result', {{method:'POST', body:'ERROR:' + error.name + ':' + error.message}});
      }}
      document.title = 'PUSH_TEST_DONE';
    }})();
    </script>""".encode()


def main(helper, timeout):
    results = []

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/worker.js":
                content = WORKER
                content_type = "application/javascript"
            elif self.path == "/create":
                content = page("create")
                content_type = "text/html"
            elif self.path == "/check":
                content = page("check")
                content_type = "text/html"
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Service-Worker-Allowed", "/")
            self.end_headers()
            self.wfile.write(content)

        def do_POST(self):
            body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
            results.append(body.decode())
            self.send_response(204)
            self.end_headers()

        def log_message(self, *_args):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with tempfile.TemporaryDirectory(prefix="webappcontainer-push-") as temporary:
            environment = os.environ.copy()
            environment["XDG_DATA_HOME"] = str(Path(temporary) / "data")
            environment["XDG_CACHE_HOME"] = str(Path(temporary) / "cache")
            storage = Path(temporary) / "profile"
            endpoint = None
            helper_logs = []
            for mode in ("create", "check"):
                command = [str(helper), "--storage", str(storage), "--url",
                           f"http://localhost:{server.server_port}/{mode}"]
                completed = subprocess.run(
                    command,
                    env=environment, timeout=timeout, capture_output=True, text=True)
                if completed.returncode:
                    raise RuntimeError(
                        f"push helper {mode} failed ({completed.returncode})\n"
                        f"{completed.stdout}\n{completed.stderr}")
                helper_logs.append(completed.stderr)
                if len(results) != (1 if mode == "create" else 2):
                    raise RuntimeError(f"push helper {mode} did not report a result")
                result = results[-1]
                if result.startswith("ERROR:") or result == "MISSING":
                    raise RuntimeError(
                        f"push helper {mode} returned {result}\n" +
                        "\n".join(helper_logs))
                if endpoint is None:
                    endpoint = result
                elif not result:
                    raise RuntimeError("restored push subscription endpoint is empty")
            print(json.dumps({"endpoint_restored": True}))
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("helper", type=Path)
    parser.add_argument("--timeout", type=float, default=75)
    args = parser.parse_args()
    raise SystemExit(main(args.helper.resolve(strict=True), args.timeout))
