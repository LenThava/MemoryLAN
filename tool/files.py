#!/usr/bin/env python3

import argparse
import binascii
import json
import os
from pathlib import Path
import re
import select
import termios
import time
import zlib

from lab import exchange


MAX_BYTES = 65536
MAX_WIRE = 131072


def node_id(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{4}", value):
        raise ValueError("Invalid board identity")
    return value


def mac_id(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{12}", value):
        raise ValueError("Invalid hardware MAC")
    return value


def number(value, maximum=0xffffffff):
    if type(value) is not int or not 0 < value <= maximum:
        raise ValueError("Invalid run number")
    return value


def file_info(info, node=None, mac=None):
    if not isinstance(info, dict):
        raise ValueError("Invalid file information")
    node = node_id(info.get("node") if node is None else node)
    if "node" in info and info["node"] != node:
        raise ValueError("File node differs from board identity")
    if "mac" in info:
        value = mac_id(info["mac"])
        if mac is not None and value != mac:
            raise ValueError("File MAC differs from board identity")
        mac = value
    if mac is not None:
        mac_id(mac)
    run = number(info.get("run"))
    names = {f"{node}-{run}.log"}
    if mac is not None:
        names.update((f"{mac}-{run}.log", f"{mac}-{run}.top"))
    if info.get("name") not in names:
        raise ValueError("Invalid filename or file identity")
    kind = "topology" if info["name"].endswith(".top") else "result"
    if info.get("type", kind) != kind:
        raise ValueError("File type differs from filename")
    length = info.get("bytes")
    if type(length) is not int or not 0 <= length <= MAX_BYTES:
        raise ValueError("Invalid file length")
    if not isinstance(info.get("crc32"), str) or not re.fullmatch(r"[0-9a-f]{8}", info["crc32"]):
        raise ValueError("Invalid file checksum")
    return info


def identify(port):
    reply = exchange(port, "id")
    if not isinstance(reply, dict):
        raise ValueError("Invalid identity response")
    node_id(reply.get("node"))
    if "mac" in reply:
        mac_id(reply["mac"])
    if reply.get("ok") is not True or type(reply.get("files_ok")) is not bool:
        raise ValueError("Invalid identity response")
    return reply


def list_files(port):
    reply = exchange(port, "files")
    if not isinstance(reply, dict) or reply.get("ok") is not True or reply.get("kind") != "mlan-files":
        raise ValueError("Invalid file-list response")
    node = node_id(reply.get("node"))
    mac = mac_id(reply["mac"]) if "mac" in reply else None
    entries = reply.get("files")
    if not isinstance(entries, list):
        raise ValueError("Invalid file list")
    names = set()
    for info in entries:
        file_info(info, node, mac)
        if info["name"] in names:
            raise ValueError("Duplicate filename in file list")
        names.add(info["name"])
    return reply


def next_line(lines):
    try:
        return next(lines)
    except StopIteration as error:
        raise ValueError("Truncated uuencoded file") from error


def decode_uu(lines, info):
    lines = iter(lines)
    if next_line(lines) != f'begin 644 {info["name"]}'.encode("ascii"):
        raise ValueError("Invalid uuencode header")
    data = bytearray()
    short_line = False
    while True:
        line = next_line(lines)
        if not line or any(c < 32 or c > 96 for c in line):
            raise ValueError("Invalid uuencode characters")
        count = (line[0] - 32) & 63
        if count > 45 or len(line) != 1 + 4 * ((count + 2) // 3):
            raise ValueError("Invalid uuencode line length")
        if count == 0:
            break
        if short_line:
            raise ValueError("Data after final uuencode data line")
        short_line = count < 45
        decoded = binascii.a2b_uu(line)
        if len(decoded) != count:
            raise ValueError("Invalid decoded line length")
        data.extend(decoded)
        if len(data) > MAX_BYTES or len(data) > info["bytes"]:
            raise ValueError("File exceeds advertised length")
    if next_line(lines) != b"end":
        raise ValueError("Missing uuencode end marker")
    if len(data) != info["bytes"]:
        raise ValueError("File length mismatch")
    if f"{zlib.crc32(data):08x}" != info["crc32"]:
        raise ValueError("File checksum mismatch")
    return bytes(data)


def read_transfer(lines, run, topology=False):
    run = number(run)
    lines = iter(lines)
    for line in lines:
        if not line.startswith(b"@MLAN "):
            continue
        info = json.loads(line[6:])
        if not isinstance(info, dict):
            raise ValueError("Invalid file response")
        if info.get("ok") is False:
            raise RuntimeError(info.get("message", "Board rejected file request"))
        if info.get("kind") != "mlan-file":
            continue
        if info.get("ok") is not True:
            raise ValueError("Invalid file response")
        file_info(info)
        if info["run"] != run:
            raise ValueError("Board returned another run")
        if info["name"].endswith(".top") != topology:
            raise ValueError("Board returned another file type")
        data = decode_uu(lines, info)
        record = json.loads(data)
        expected = "mlan-topology" if topology else "mlan-run"
        if (not isinstance(record, dict) or record.get("kind") != expected or
                type(record.get("run")) is not int or record["run"] != run or
                record.get("node") != info["node"]):
            raise ValueError("Log identity does not match transfer envelope")
        if "mac" in info and record.get("mac") != info["mac"]:
            raise ValueError("Log MAC does not match transfer envelope")
        if "mac" in record:
            mac_id(record["mac"])
            if "mac" not in info:
                raise ValueError("Transfer envelope is missing the log MAC")
        if topology and (type(record.get("schema")) is not int or record["schema"] != 1 or "mac" not in info):
            raise ValueError("Invalid topology log schema or identity")
        return info, data
    raise ValueError("No file response; check firmware and command")


def serial_lines(fd, deadline):
    pending = b""
    received = 0
    while time.monotonic() < deadline:
        ready, _, _ = select.select([fd], [], [], min(0.25, max(0, deadline - time.monotonic())))
        if not ready:
            continue
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not chunk:
            raise RuntimeError("Board disconnected before file transfer finished")
        received += len(chunk)
        if received > MAX_WIRE:
            raise ValueError("Unexpected serial output exceeds transfer limit")
        pending += chunk
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            yield line.removesuffix(b"\r")
    raise RuntimeError("File transfer timed out; no complete file was saved")


def fetch_file(port, run, topology=False):
    if not re.fullmatch(r"/dev/(cu|tty)\.usbmodem[\w.-]+", port):
        raise ValueError("Use the exact USB-modem port reported by ./go port")
    run = number(run)
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        attrs[0] = attrs[1] = attrs[3] = 0
        attrs[2] = termios.CS8 | termios.CLOCAL | termios.CREAD
        attrs[4] = attrs[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        termios.tcflush(fd, termios.TCIFLUSH)
        deadline = time.monotonic() + 30
        command = f'{"topo" if topology else "dump"} {run}\n'.encode("ascii")
        while command:
            if time.monotonic() >= deadline:
                raise RuntimeError("Timed out sending file request")
            _, ready, _ = select.select([], [fd], [], 0.25)
            if ready:
                try:
                    command = command[os.write(fd, command):]
                except BlockingIOError:
                    continue
        return read_transfer(serial_lines(fd, deadline), run, topology)
    finally:
        os.close(fd)


def save_file(info, data, directory):
    file_info(info)
    if len(data) != info["bytes"] or f"{zlib.crc32(data):08x}" != info["crc32"]:
        raise ValueError("Refusing to save an unchecked file")
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / info["name"]
    while True:
        try:
            with path.open("xb") as file:
                file.write(data)
            return path
        except FileExistsError:
            name = Path(info["name"])
            path = directory / f'{name.stem}-{time.time_ns()}{name.suffix}'


def main():
    parser = argparse.ArgumentParser(description="MemoryLAN log retrieval")
    sub = parser.add_subparsers(dest="action", required=True)
    for action in ("id", "list", "get"):
        command = sub.add_parser(action)
        command.add_argument("port")
        if action == "get":
            command.add_argument("run", type=int)
            command.add_argument("--out", default="log")
            command.add_argument("--topology", action="store_true")
    args = parser.parse_args()
    try:
        if args.action == "get":
            info, data = fetch_file(args.port, args.run, args.topology)
            print(save_file(info, data, args.out))
        else:
            print(json.dumps(identify(args.port) if args.action == "id" else list_files(args.port), indent=2))
    except (ValueError, OSError, RuntimeError, binascii.Error) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
