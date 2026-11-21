#!/usr/bin/env python3
"""Run the stoprock firmware in Espressif's esp-emulator and show it in your browser.

    python3 simulator/run.py            then open http://localhost:8200

Needs esp-emu (https://github.com/espressif/esp-emulator; the installer puts it in
~/.local/bin) and the simulator firmware image, built with `pio run -e emu` in firmware/.
Uses only the Python standard library.
"""
import argparse
import json
import os
import queue
import shutil
import socket
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import urllib.request
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
DEFAULT_FIRMWARE = ROOT / "firmware" / ".pio" / "build" / "emu" / "stoprock-emu.bin"
PART = HERE / "parts" / "pushbutton.wasm"
BUTTONS = ("top", "side", "up", "press", "down", "lowbat")
W = H = 200
STRIDE = W // 8


class Device:
    """The emulator process, its control channel and what it last drew."""

    def __init__(self, esp_emu, firmware, control_port):
        self.fb = bytearray(STRIDE * H)
        self.lock = threading.Lock()
        self.listeners = []
        self.history = []  # recent console lines, replayed to a page that connects later
        self.control_port = control_port
        self.control = None
        cmd = [esp_emu, "--chip", "esp32s3", "--firmware", str(firmware),
               "--cosim-board", str(HERE / "board.json"),
               "--control-tcp", f"127.0.0.1:{control_port}",
               # set the clock once the firmware is up
               "--inject-on", "stoprock ready", "--inject", f"t {int(time.time())}\\n"]
        print("starting:", " ".join(cmd))
        self.proc = subprocess.Popen(cmd, cwd=HERE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, bufsize=1)
        threading.Thread(target=self._read, daemon=True).start()

    # -- emulator output
    def _read(self):
        for line in self.proc.stdout:
            line = line.rstrip("\n")
            if line.startswith("@FB "):
                self._framebuffer(line)
            elif line.startswith("@TONE "):
                self.emit({"type": "tone", "hz": int(line.split()[1])})
            elif line.startswith("@LED "):
                self.emit({"type": "led", "on": line.split()[1] == "1"})
            else:
                print(line)
                self.history = (self.history + [line])[-300:]
                self.emit({"type": "log", "text": line})
        self.emit({"type": "log", "text": f"emulator exited ({self.proc.wait()})"})

    def _framebuffer(self, line):
        try:
            _, x, y, w, h, full, data = line.split(" ", 6)
            x, y, w, h = int(x), int(y), int(w), int(h)
            raw = bytes.fromhex(data.strip())
        except ValueError:
            return
        bpr = w // 8
        with self.lock:
            for row in range(h):
                start = (y + row) * STRIDE + x // 8
                self.fb[start:start + bpr] = raw[row * bpr:(row + 1) * bpr]
            snapshot = self.fb.hex()
        self.emit({"type": "fb", "full": full == "1", "data": snapshot, "x": x, "y": y, "w": w, "h": h})

    def emit(self, event):
        for q in list(self.listeners):
            q.put(event)

    # -- control channel
    def send(self, command):
        for attempt in range(2):
            try:
                if self.control is None:
                    self.control = socket.create_connection(("127.0.0.1", self.control_port), timeout=5)
                    self.control_file = self.control.makefile("r")
                self.control.sendall((command + "\n").encode())
                return self.control_file.readline().strip()
            except OSError:
                self.control = None
                if attempt:
                    raise
                time.sleep(0.3)

    def stop(self):
        self.proc.terminate()


PAGE = (HERE / "viewer.html").read_text()


def make_handler(device):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            if self.path == "/":
                body = PAGE.encode()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            elif self.path == "/events":
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.end_headers()
                q = queue.Queue()
                device.listeners.append(q)
                for text in device.history:
                    q.put({"type": "log", "text": text})
                with device.lock:
                    q.put({"type": "fb", "full": False, "data": device.fb.hex()})
                try:
                    while True:
                        try:
                            event = q.get(timeout=15)
                            self.wfile.write(f"data: {json.dumps(event)}\n\n".encode())
                        except queue.Empty:
                            self.wfile.write(b": keep-alive\n\n")
                        self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError):
                    pass
                finally:
                    device.listeners.remove(q)
            else:
                self.send_error(404)

        def do_POST(self):
            if self.path != "/button":
                self.send_error(404)
                return
            body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            name, action = body.get("name"), body.get("action")
            if name not in BUTTONS or action not in ("press", "release"):
                self.send_error(400)
                return
            try:
                answer = device.send(f"part {name} input {action}")
            except OSError as e:
                answer = f"err: {e}"
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"answer": answer}).encode())

    return Handler


def find_esp_emu(given):
    for candidate in (given, shutil.which("esp-emu"), str(Path.home() / ".local" / "bin" / "esp-emu")):
        if candidate and Path(candidate).is_file():
            return candidate
    sys.exit("esp-emu not found. Install it with:\n"
             "  curl -fsSL https://raw.githubusercontent.com/espressif/esp-emulator/main/install.sh | sh")


def ensure_part():
    """Fetch pushbutton.wasm from the esp-emulator examples kit the first time."""
    if PART.exists():
        return
    print("fetching the push-button part from the esp-emulator examples kit...")
    with urllib.request.urlopen("https://api.github.com/repos/espressif/esp-emulator/releases/latest") as r:
        release = json.load(r)
    url = next(a["browser_download_url"] for a in release["assets"] if a["name"].endswith("-examples.tar.gz"))
    with tempfile.TemporaryDirectory() as tmp:
        kit = Path(tmp) / "examples.tar.gz"
        urllib.request.urlretrieve(url, kit)
        with tarfile.open(kit) as tar:
            member = next(m for m in tar.getmembers() if m.name.endswith("/pushbutton.wasm") or m.name == "pushbutton.wasm")
            data = tar.extractfile(member).read()
    PART.parent.mkdir(exist_ok=True)
    PART.write_bytes(data)
    print("saved", PART.relative_to(ROOT))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--firmware", default=str(DEFAULT_FIRMWARE), help="merged flash image (default: %(default)s)")
    ap.add_argument("--esp-emu", default=None, help="path to the esp-emu binary")
    ap.add_argument("--port", type=int, default=8200, help="web page port (default: %(default)s)")
    ap.add_argument("--control-port", type=int, default=5556, help="emulator control port (default: %(default)s)")
    ap.add_argument("--no-browser", action="store_true", help="don't open the page automatically")
    args = ap.parse_args()

    firmware = Path(args.firmware)
    if not firmware.exists():
        sys.exit(f"{firmware} not found: build it with `pio run -e emu` in firmware/")
    esp_emu = find_esp_emu(args.esp_emu)
    ensure_part()

    device = Device(esp_emu, firmware, args.control_port)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(device))
    url = f"http://localhost:{args.port}"
    print(f"stoprock simulator: {url}  (Ctrl-C to stop)")
    if not args.no_browser:
        threading.Timer(1.0, webbrowser.open, [url]).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        device.stop()


if __name__ == "__main__":
    main()
