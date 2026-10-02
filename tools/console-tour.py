#!/usr/bin/env python3
# ps5-homebrew-ui - One validation run on a console: install, tour, collect evidence.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs the app's self-driving tour on a console and brings the evidence back.

usage: PS5_HOST=<address> tools/console-tour.py <results dir> [design id|all]

Steps: check the console's services; refuse if the title is running; upload
dist/<TITLE_ID> (every file verified by hash); wait for its registration;
put the tour request beside it; wait for its registration; record the kernel
log; launch the title; wait for its report; download the report, the pictures
and the app log from the title's storage; wait for the app to close itself;
remove the request; check everything.

The script never kills the app and never retries. The only thing it deletes
on the console is its own request file. If something is wrong it stops and says what it saw.

Environment:
  PS5_HOST          console address (required)
  FTP_PORT          default 2121
  KLOG_PORT         default 3232 (kernel log; skipped if closed)
  ELF_PORT          default 9021 (ELF loader, for the launch helper)
  PS5_PROTOCOL      path of ps5-homebrew-dev-protocol (default: beside this repository)
  PS5_PAYLOAD_SDK   default .deps/native/ps5-payload-sdk
  TOUR_TIMEOUT      seconds to wait for the tour (default 2400)
  SETTLE_SECONDS    pause before the installed files are verified again (default 120)
  REGISTER_TIMEOUT  seconds to wait for the title to be registered (default 120)
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
    """One FTP session.

    Signed files are compared as the bytes that are stored. Some FTP servers
    for the console have a "SELF" switch that makes them hand out the
    decrypted ELF instead: it must stay off, or every executable reads back
    larger than, and different from, what was uploaded.
    """

    def __init__(self, host, port):
        self.ftp = FTP()
        self.ftp.connect(host, port, timeout=30)
        self.ftp.login("anonymous", "ps5-homebrew-ui")

    def close(self):
        try:
            self.ftp.quit()
        except all_errors:
            pass

    def names(self, path):
        # The console's FTP server lists only the current directory: an
        # argument to MLSD is ignored, so change into the folder first.
        try:
            self.ftp.cwd(path)
            return [name for name, _ in self.ftp.mlsd() if name not in (".", "..")]
        except all_errors:
            return None
        finally:
            try:
                self.ftp.cwd("/")
            except all_errors:
                pass

    def read(self, path):
        data = io.BytesIO()
        try:
            self.ftp.retrbinary(f"RETR {path}", data.write)
        except all_errors:
            return None
        return data.getvalue()

    def stamp(self, path):
        """Modification time and size of a file, or None (the server has no MDTM)."""
        directory, name = path.rsplit("/", 1)
        try:
            self.ftp.cwd(directory)
            for entry, facts in self.ftp.mlsd():
                if entry == name:
                    return (facts.get("modify"), facts.get("size"))
            return None
        except all_errors:
            return None
        finally:
            try:
                self.ftp.cwd("/")
            except all_errors:
                pass

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
    timeout = int(os.environ.get("TOUR_TIMEOUT", "2400"))
    settle = int(os.environ.get("SETTLE_SECONDS", "120"))
    register_wait = int(os.environ.get("REGISTER_TIMEOUT", "120"))
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

    # The request goes into the install folder (the app reads it as
    # /app0/dev/request.txt): a running title's own storage can be read over
    # FTP but not written. The token makes the app honour it once.
    token = str(int(time.time()))
    console.write(f"{remote}/dev/request.txt", f"tour {only} {token}\n".encode())
    uploaded += 1
    console.close()

    # ShadowMount Plus registers a new title only once its folder has stopped
    # changing. Launching before that shows "title not found" on the console.
    waited = 0
    while True:
        console = Console(host, ftp_port)
        registered = "mount.lnk" in (console.names(f"/user/app/{title}") or [])
        console.close()
        if registered:
            break
        if waited >= register_wait:
            stop(f"{title} was not registered after {register_wait} s "
                 "(is ShadowMount Plus running?); nothing was launched")
        if waited == 0:
            say("Waiting for the console to register the title")
        time.sleep(5)
        waited += 5
    if first_install or uploaded:
        time.sleep(15)  # let the registration settle before the launch

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

    # The app's own storage is mounted only while the title runs, and a PC
    # can read it through the title's sandbox. So: wait for the app to come
    # up and take the request, wait for its report, copy the evidence, and
    # wait for the app to close itself (it does a few minutes after the tour).
    dev = f"/mnt/sandbox/{title}_000/download0/hui/dev"
    report_path = f"{dev}/tour/report.txt"

    def connect():
        try:
            return Console(host, ftp_port)
        except all_errors:
            done.set()
            stop("the console stopped answering during the run; do not retry, look at klog.txt")

    taken = False
    while time.time() - started < 90:
        time.sleep(3)
        console = connect()
        taken = (console.read(f"{dev}/handled.txt") or b"").decode().strip() == token
        console.close()
        if taken:
            break
    if not taken:
        done.set()
        stop("the app did not take the tour request (it may be running: close it by hand)")
    say("The app is up and touring")

    finished = False
    while time.time() - started < timeout:
        time.sleep(5)
        console = connect()
        active = running_titles(console)
        reported = console.stamp(report_path) is not None
        console.close()
        if title not in active:
            done.set()
            stop("the title closed before its tour reported; look at klog.txt")
        if reported:
            finished = True
            break
    if not finished:
        done.set()
        stop(f"no finished tour after {timeout} s; the app was left as it is")
    say(f"Tour finished after {time.time() - started:.0f} s; collecting")

    console = connect()
    data = console.read(report_path)
    if data is None:
        done.set()
        stop("report.txt could not be read back; the app closes by itself in a few minutes")
    (results / "report.txt").write_bytes(data)
    log = console.read(f"{dev}/app.log") or b""
    (results / "app.log").write_bytes(log)
    pictures = []
    try:
        console.ftp.cwd(f"{dev}/tour")
        pictures = sorted(name for name, _ in console.ftp.mlsd() if name.endswith(".bmp"))
        console.ftp.cwd("/")
    except all_errors:
        pass
    for name in pictures:
        picture = console.read(f"{dev}/tour/{name}")
        if picture:
            (results / "tour" / name).write_bytes(picture)
    lifecycle = console.read("/data/shadowmount/debug.log") or b""
    (results / "shadowmount.txt").write_text("".join(
        line + "\n" for line in lifecycle.decode("utf-8", "replace").splitlines() if title in line))
    console.close()
    say(f"Downloaded the report, the log and {len(pictures)} pictures to {results}")

    # The app closes itself once its evidence window is over.
    say("Waiting for the app to close itself")
    closed = False
    for _ in range(72):
        time.sleep(5)
        console = connect()
        closed = title not in running_titles(console)
        console.close()
        if closed:
            break
    time.sleep(3)
    done.set()
    if klog is not None:
        klog.join(timeout=5)
    say("The app closed itself" if closed else "The app is still running: close it from the console")
    if closed:
        # The request has been honoured; the install folder goes back to what was built.
        console = connect()
        try:
            console.ftp.sendcmd(f"DELE {remote}/dev/request.txt")
            console.ftp.sendcmd(f"RMD {remote}/dev")
        except all_errors:
            pass
        console.close()

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
    for needed in ("first-swap ok", "tour finished"):
        if needed not in text:
            problems.append(f"app.log: no '{needed}' line")
    if not closed:
        problems.append("the app did not close after its evidence window")
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
