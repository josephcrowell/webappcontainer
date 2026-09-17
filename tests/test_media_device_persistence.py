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


def page(mode):
    return f"""<!doctype html><title>Running</title><script>
    (async () => {{
      let stream = null;
      if ({json.dumps(mode)} === 'create') {{
        try {{
          stream = await navigator.mediaDevices.getUserMedia({{audio:true, video:true}});
        }} catch (_) {{
          try {{ stream = await navigator.mediaDevices.getUserMedia({{audio:true}}); }}
          catch (_) {{}}
        }}
      }}
      const devices = (await navigator.mediaDevices.enumerateDevices()).map(device => ({{
        kind: device.kind,
        deviceId: device.deviceId,
        groupId: device.groupId,
        label: device.label
      }}));
      let selected;
      if ({json.dumps(mode)} === 'create') {{
        const choosePhysical = kind => {{
          const candidates = devices.filter(device => device.kind === kind && device.deviceId);
          return candidates.find(device =>
            device.deviceId !== 'default' && device.deviceId !== 'communications') ||
            candidates[0] || {{deviceId:''}};
        }};
        selected = {{
          microphone: choosePhysical('audioinput').deviceId,
          speaker: choosePhysical('audiooutput').deviceId,
          camera: choosePhysical('videoinput').deviceId
        }};
        localStorage.setItem('webappcontainer-media-selection-test', JSON.stringify(selected));
      }} else {{
        selected = JSON.parse(localStorage.getItem('webappcontainer-media-selection-test') || 'null');
      }}
      if (stream)
        stream.getTracks().forEach(track => track.stop());
      await fetch('/result', {{method:'POST', body:JSON.stringify({{devices, selected}})}});
      document.title = 'PUSH_TEST_DONE';
    }})();
    </script>""".encode()


def main(helper, timeout):
    results = []

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path not in ("/create", "/check"):
                self.send_error(404)
                return
            content = page(self.path[1:])
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)

        def do_POST(self):
            body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
            results.append(json.loads(body))
            self.send_response(204)
            self.end_headers()

        def log_message(self, *_args):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with tempfile.TemporaryDirectory(prefix="webappcontainer-media-") as temporary:
            environment = os.environ.copy()
            environment["XDG_DATA_HOME"] = str(Path(temporary) / "data")
            environment["XDG_CACHE_HOME"] = str(Path(temporary) / "cache")
            storage = Path(temporary) / "profile"
            for mode in ("create", "check"):
                command = [str(helper), "--storage", str(storage), "--url",
                           f"http://localhost:{server.server_port}/{mode}",
                           "--cxx-profile"]
                if mode == "check":
                    command.append("--pregrant-media")
                completed = subprocess.run(
                    command,
                    env=environment, timeout=timeout, capture_output=True, text=True)
                if completed.returncode:
                    raise RuntimeError(
                        f"media helper {mode} failed ({completed.returncode})\n"
                        f"{completed.stdout}\n{completed.stderr}")
                if len(results) != (1 if mode == "create" else 2):
                    raise RuntimeError(f"media helper {mode} did not report a result")

            created, restored = results
            if not created["selected"]:
                raise RuntimeError("create process did not store media selections")
            if restored["selected"] != created["selected"]:
                raise RuntimeError(
                    f"stored media selection changed across restart: "
                    f"{created['selected']} != {restored['selected']}")
            def physical_devices(result):
                return {
                    (device["kind"], device["deviceId"])
                    for device in result["devices"]
                    if device["deviceId"] not in ("", "default", "communications")
                }
            created_physical = physical_devices(created)
            restored_physical = physical_devices(restored)
            if not created_physical:
                raise RuntimeError("no physical media device IDs were available to verify")
            if restored_physical != created_physical:
                changed = []
                for kind in ("audioinput", "audiooutput", "videoinput"):
                    before_ids = {device_id for device_kind, device_id
                                  in created_physical if device_kind == kind}
                    after_ids = {device_id for device_kind, device_id
                                 in restored_physical if device_kind == kind}
                    if before_ids != after_ids:
                        changed.append(
                            f"{kind} ({len(before_ids)} before, {len(after_ids)} after)")
                raise RuntimeError(
                    "physical media device IDs changed across restart: " +
                    ", ".join(changed))
            restored_by_kind = {
                device["kind"]: {item["deviceId"] for item in restored["devices"]
                                  if item["kind"] == device["kind"]}
                for device in restored["devices"]
            }
            checked = 0
            for selection, kind in (("microphone", "audioinput"),
                                    ("speaker", "audiooutput"),
                                    ("camera", "videoinput")):
                selected_id = created["selected"].get(selection, "")
                if not selected_id:
                    continue
                checked += 1
                if selected_id not in restored_by_kind.get(kind, set()):
                    raise RuntimeError(
                        f"{selection} device ID changed across restart")
            if not checked:
                raise RuntimeError("no nonempty media device IDs were available to verify")
            print(json.dumps({"stable_selected_devices": checked,
                              "stable_physical_devices": len(created_physical)}))
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
