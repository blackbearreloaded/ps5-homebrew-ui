#!/usr/bin/env python3
# ps5-homebrew-ui - One validation run on a console: install, tour, collect evidence.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs the app's self-driving tour on a console and brings the evidence back.

usage: PS5_HOST=<address> tools/console-tour.py <results dir> [design id|all]

Steps: check the console's services; refuse if the title is running; upload
dist/<TITLE_ID> (every file verified by hash); write the tour trigger; record
the kernel log; launch the title; wait for the app to finish its tour and
close itself; download the report, the pictures and the app log; check them.

The script never kills the app, never retries and never deletes anything on
the console. If something is wrong it stops and says what it saw.

Environment:
  PS5_HOST          console address (required)
  FTP_PORT          default 2121
  KLOG_PORT         default 3232 (kernel log; skipped if closed)
  ELF_PORT          default 9021 (ELF loader, for the launch helper)
  PS5_PROTOCOL      path of ps5-homebrew-dev-protocol (default: beside this repository)
  PS5_PAYLOAD_SDK   default .deps/native/ps5-payload-sdk
  TOUR_TIMEOUT      seconds to wait for the tour (default 900)
  SETTLE_SECONDS    pause before the installed files are verified again (default 120)
"""

import hashlib
import io
import json
import os
import socket
import subprocess
import sys
import threading
import time
from ftplib import FTP, all_errors, error_perm
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEV_ROOT = "/data/ps5-homebrew-ui"
BAD_LOG_WORDS = ("fatal", "failed", "rejected", "refused", "assertion")
BAD_KLOG_WORDS = ("panic", "crash", "coredump", "segv", "sigsegv")


def say(text):
    print(f"==> {text}", flush=True)


def stop(text):
    raise SystemExit(f"STOPPED: {text}")


def port_open(host, port):
    try:
        with socket.create_connection((host, port), timeout=5):
            return True
    except OSError:
        return False


class Console:
    """One FTP session. Signed files are read back as stored bytes (SELF)."""

    def __init__(self, host, port):
        self.ftp = FTP()
        self.ftp.connect(host, port, timeout=30)
        self.ftp.login("anonymous", "ps5-homebrew-ui")
        try:
            self.ftp.sendcmd("SELF")
        except all_errors:
            pass  # other FTP servers have no such switch

    def close(self):
        try:
            self.ftp.quit()
        except all_errors:
            pass

    def names(self, path):
        try:
            return [name for name, _ in self.ftp.mlsd(path) if name not in (".", "..")]
        except all_errors:
            return None

    def read(self, path):
        data = io.BytesIO()
        try:
            self.ftp.retrbinary(f"RETR {path}", data.write)
        except all_errors:
            return None
        return data.getvalue()

    def stamp(self, path):
        try:
            return self.ftp.sendcmd(f"MDTM {path}")
        except all_errors:
            return None

    def make_dirs(self, path):
        current = ""
        for part in path.strip("/").split("/"):
            current += "/" + part
            try:
                self.ftp.mkd(current)
            except error_perm:
                pass

    def write(self, path, data):
        """Upload under a temporary name, verify by hash, then rename."""
        directory, name = path.rsplit("/", 1)
        temporary = f"{directory}/.{name}.upload"
        self.make_dirs(directory)
        self.ftp.storbinary(f"STOR {temporary}", io.BytesIO(data), blocksize=256 * 1024)
        back = self.read(temporary)
        if back is None or hashlib.sha256(back).digest() != hashlib.sha256(data).digest():
            stop(f"upload of {path} did not read back identically")
        try:
            self.ftp.sendcmd(f"DELE {path}")  # only the file this upload replaces
        except all_errors:
            pass
        self.ftp.rename(temporary, path)


def running_titles(console):
    names = console.names("/mnt/sandbox") or []
    return sorted({name.split("_")[0] for name in names if name.startswith("PPSA")})


def record_klog(host, port, path, done):
    try:
        with socket.create_connection((host, port), timeout=5) as link, open(path, "wb") as out:
            link.settimeout(1.0)
            while not done.is_set():
                try:
                    chunk = link.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                out.write(chunk)
                out.flush()
    except OSError as error:
        Path(path).write_text(f"kernel log not recorded: {error}\n")


def main():
    if len(sys.argv) not in (2, 3):
        raise SystemExit(__doc__)
    results = Path(sys.argv[1])
    only = sys.argv[2] if len(sys.argv) == 3 else "all"
    host = os.environ.get("PS5_HOST") or stop("set PS5_HOST")
    ftp_port = int(os.environ.get("FTP_PORT", "2121"))
    klog_port = int(os.environ.get("KLOG_PORT", "3232"))
    elf_port = int(os.environ.get("ELF_PORT", "9021"))
    timeout = int(os.environ.get("TOUR_TIMEOUT", "900"))
    settle = int(os.environ.get("SETTLE_SECONDS", "120"))
    protocol = Path(os.environ.get("PS5_PROTOCOL", ROOT.parent / "ps5-homebrew-dev-protocol"))
    sdk = os.environ.get("PS5_PAYLOAD_SDK", str(ROOT / ".deps/native/ps5-payload-sdk"))
    sender = protocol / "scripts/send-controller.sh"
    title = json.loads((ROOT / "sce_sys/param.json").read_text())["titleId"]
    package = ROOT / "dist" / title
    if not (package / "eboot.bin").is_file():
        stop(f"{package} is not built (run make)")
    if not sender.is_file():
        stop(f"launch helper not found: {sender} (set PS5_PROTOCOL)")
    results.mkdir(parents=True, exist_ok=True)
    (results / "tour").mkdir(exist_ok=True)

    say(f"Checking the console's services at {host}")
    if not port_open(host, ftp_port) or not port_open(host, elf_port):
        stop("FTP or the ELF loader does not answer")
    console = Console(host, ftp_port)
    active = running_titles(console)
    if title in active:
        stop(f"{title} is running on the console; close it first")
    if active:
        say(f"Other titles running (left alone): {', '.join(active)}")

    remote = f"/data/homebrew/{title}"
    first_install = console.names(remote) is None
    files = sorted(p for p in package.rglob("*") if p.is_file())
    # The executable and the metadata go last: the title is complete when they land.
    files.sort(key=lambda p: (p.name in ("eboot.bin", "param.json"), str(p)))
    uploaded = 0
    manifest = {}
    for path in files:
        relative = path.relative_to(package).as_posix()
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        manifest[relative] = digest
        installed = console.read(f"{remote}/{relative}")
        if installed is not None and hashlib.sha256(installed).hexdigest() == digest:
            continue
        console.write(f"{remote}/{relative}", data)
        uploaded += 1
    say(f"Installed {title}: {len(files)} files verified, {uploaded} uploaded")
    (results / "installed.sha256").write_text(
        "".join(f"{digest}  {name}\n" for name, digest in sorted(manifest.items())))

    report_path = f"{DEV_ROOT}/tour/report.txt"
    before = console.stamp(report_path)
    console.write(f"{DEV_ROOT}/tour.txt", (only + "\n").encode())
    console.close()
    if first_install:
        say("First install: giving the console 30 s to register the title")
        time.sleep(30)

    done = threading.Event()
    klog = None
    if port_open(host, klog_port):
        klog = threading.Thread(target=record_klog,
                                args=(host, klog_port, results / "klog.txt", done), daemon=True)
        klog.start()
        time.sleep(1.0)

    say(f"Launching {title}")
    started = time.time()
    launch = subprocess.run(["bash", str(sender), "launch", title, host, str(elf_port)],
                            env={**os.environ, "PS5_PAYLOAD_SDK": sdk}, capture_output=True,
                            text=True, check=False)
    if launch.returncode != 0:
        done.set()
        stop(f"the launch helper failed: {launch.stderr.strip() or launch.stdout.strip()}")

    seen_running = False
    finished = False
    while time.time() - started < timeout:
        time.sleep(5)
        try:
            console = Console(host, ftp_port)
        except all_errors:
            done.set()
            stop("the console stopped answering during the run; do not retry, look at klog.txt")
        active = running_titles(console)
        changed = console.stamp(report_path) not in (None, before)
        console.close()
        seen_running = seen_running or title in active
        if not seen_running and time.time() - started > 60:
            done.set()
            stop("the title was never seen running (not registered, or it failed to start)")
        if changed and title not in active:
            finished = True
            break
    elapsed = time.time() - started
    time.sleep(3)
    done.set()
    if klog is not None:
        klog.join(timeout=5)
    if not finished:
        stop(f"no finished tour after {timeout} s; the app was left as it is")
    say(f"Tour finished and the app closed itself after {elapsed:.0f} s")

    console = Console(host, ftp_port)
    for name in ("report.txt",):
        data = console.read(f"{DEV_ROOT}/tour/{name}")
        if data is None:
            stop(f"{name} could not be read back")
        (results / name).write_bytes(data)
    log = console.read(f"{DEV_ROOT}/app.log") or b""
    (results / "app.log").write_bytes(log)
    pictures = [n for n in (console.names(f"{DEV_ROOT}/tour") or []) if n.endswith(".bmp")]
    for name in sorted(pictures):
        data = console.read(f"{DEV_ROOT}/tour/{name}")
        if data:
            (results / "tour" / name).write_bytes(data)
    console.close()
    say(f"Downloaded the report, the log and {len(pictures)} pictures to {results}")

    try:
        from PIL import Image
        for bmp in sorted((results / "tour").glob("*.bmp")):
            Image.open(bmp).save(bmp.with_suffix(".png"))
            bmp.unlink()
    except ImportError:
        pass

    problems = []
    text = log.decode("utf-8", "replace")
    for line in text.splitlines():
        if any(word in line.lower() for word in BAD_LOG_WORDS) and "rejected=0" not in line:
            problems.append(f"app.log: {line.strip()}")
    for needed in ("first-swap ok", "tour finished", "quit requested"):
        if needed not in text:
            problems.append(f"app.log: no '{needed}' line")
    klog_file = results / "klog.txt"
    if klog_file.is_file():
        for line in klog_file.read_text(errors="replace").splitlines():
            if title in line and any(word in line.lower() for word in BAD_KLOG_WORDS):
                problems.append(f"klog: {line.strip()[:200]}")
    report = (results / "report.txt").read_text().strip().splitlines()
    print("\n".join(report))

    if settle > 0:
        say(f"Waiting {settle} s, then verifying the installed files once more")
        time.sleep(settle)
        console = Console(host, ftp_port)
        for relative, digest in manifest.items():
            data = console.read(f"{remote}/{relative}")
            if data is None or hashlib.sha256(data).hexdigest() != digest:
                problems.append(f"installed file changed or unreadable: {relative}")
        console.close()
    if not all(port_open(host, port) for port in (ftp_port, elf_port)):
        problems.append("the console's services do not all answer after the run")

    if problems:
        print("\n".join(problems))
        stop(f"{len(problems)} problem(s); see {results}")
    say(f"PASS: {len(report)} design(s) toured, the log is clean, the console is healthy")


if __name__ == "__main__":
    main()
