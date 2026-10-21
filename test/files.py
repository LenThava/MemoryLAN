import binascii
import copy
import json
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest.mock import patch
import zlib

sys.path.insert(0, str(Path(__file__).parents[1] / "tool"))
import files


def info_for(data, name="1234-7.log"):
    return dict(ok=True, kind="mlan-file", node="1234", run=7, name=name,
                bytes=len(data), crc32=f"{zlib.crc32(data):08x}")


def encode(data, name="1234-7.log"):
    body = [binascii.b2a_uu(data[i:i + 45], backtick=True).rstrip(b"\n")
            for i in range(0, len(data), 45)]
    return [f"begin 644 {name}".encode("ascii"), *body, b"`", b"end"]


def fixture():
    data = b'{"kind":"mlan-run","node":"1234","run":7,"received":[0,1]}\n'
    info = info_for(data)
    return info, data, [b"@MLAN " + json.dumps(info).encode("ascii"), *encode(data)]


class FileTest(unittest.TestCase):
    def test_known_cat(self):
        info = info_for(b"Cat", "cat.txt")
        lines = [b"begin 644 cat.txt", b"#0V%T", b"`", b"end"]
        self.assertEqual(files.decode_uu(lines, info), b"Cat")

    def test_all_bytes_and_boundaries(self):
        for size in (0, 1, 2, 3, 44, 45, 46, 90, 256, 4096):
            data = bytes(i % 256 for i in range(size))
            with self.subTest(size=size):
                self.assertEqual(files.decode_uu(encode(data), info_for(data)), data)
        self.assertEqual(info_for(bytes(range(256)))["crc32"], "29058c73")

    def test_complete_transfer_and_startup(self):
        info, data, lines = fixture()
        startup = [b"Starting MemoryLAN", b'@MLAN {"ok":true,"message":"trial firmware; use dump for status"}']
        self.assertEqual(files.read_transfer(startup + lines, 7), (info, data))

    def test_truncated(self):
        _, _, lines = fixture()
        for length in range(len(lines)):
            with self.subTest(length=length), self.assertRaises(ValueError):
                files.read_transfer(lines[:length], 7)

    def test_bad_checksum_and_size(self):
        info, _, lines = fixture()
        for key, value in (("crc32", "00000000"), ("bytes", info["bytes"] + 1),
                           ("bytes", info["bytes"] - 1), ("bytes", 4097), ("bytes", True)):
            altered = dict(info, **{key: value})
            wire = [b"@MLAN " + json.dumps(altered).encode(), *lines[1:]]
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                files.read_transfer(wire, 7)

    def test_bad_encoding(self):
        info, _, lines = fixture()
        for replacement in (b"", b"~", b"M", b"\x1f", b"#0V%TEXTRA", bytes([96, 160])):
            altered = copy.deepcopy(lines)
            altered[2] = replacement
            with self.subTest(replacement=replacement), self.assertRaises((ValueError, binascii.Error)):
                files.read_transfer(altered, 7)
        altered = [*lines[:-1], b"other"]
        with self.assertRaises(ValueError):
            files.read_transfer(altered, 7)
        altered = [lines[0], *encode(b"a"), b"extra"]
        with self.assertRaises(ValueError):
            files.read_transfer(altered, 7)

    def test_unsafe_or_wrong_names(self):
        info, data, _ = fixture()
        for name in ("../1234-7.log", "/tmp/1234-7.log", "1234-8.log", "abcd-7.log", "1234-7.log\nend"):
            altered = dict(info, name=name)
            wire = [b"@MLAN " + json.dumps(altered).encode(), *encode(data)]
            with self.subTest(name=name), self.assertRaises(ValueError):
                files.read_transfer(wire, 7)

    def test_header_and_request_mismatch(self):
        _, _, lines = fixture()
        with self.assertRaises(ValueError):
            files.read_transfer(lines, 8)
        altered = copy.deepcopy(lines)
        altered[1] = b"begin 644 abcd-7.log"
        with self.assertRaises(ValueError):
            files.read_transfer(altered, 7)

    def test_json_identity(self):
        for record in ({"kind": "mlan-run", "node": "abcd", "run": 7},
                       {"kind": "mlan-run", "node": "1234", "run": 8},
                       {"kind": "other", "node": "1234", "run": 7},
                       [], {"kind": "mlan-run", "node": "1234", "run": True}):
            data = json.dumps(record).encode()
            wire = [b"@MLAN " + json.dumps(info_for(data)).encode(), *encode(data)]
            with self.subTest(record=record), self.assertRaises(ValueError):
                files.read_transfer(wire, 7)

    def test_refused(self):
        with self.assertRaisesRegex(RuntimeError, "running"):
            files.read_transfer([b'@MLAN {"ok":false,"message":"running"}'], 7)

    def test_preserve_original_bytes_and_no_overwrite(self):
        info, data, _ = fixture()
        with tempfile.TemporaryDirectory(prefix="mlan-files-") as temp:
            first = files.save_file(info, data, temp)
            second = files.save_file(info, data, temp)
            self.assertNotEqual(first, second)
            self.assertEqual(first.read_bytes(), data)
            self.assertEqual(second.read_bytes(), data)
            with self.assertRaises(ValueError):
                files.save_file(info, b"changed", temp)
            self.assertEqual(len(list(Path(temp).iterdir())), 2)

    def test_list_and_identity(self):
        info, _, _ = fixture()
        entry = {key: info[key] for key in ("run", "name", "bytes", "crc32")}
        reply = dict(ok=True, kind="mlan-files", node="1234", files=[entry])
        with patch.object(files, "exchange", return_value=reply):
            self.assertEqual(files.list_files("unused"), reply)
        reply["files"] *= 2
        with patch.object(files, "exchange", return_value=reply), self.assertRaises(ValueError):
            files.list_files("unused")
        reply = dict(ok=True, node="1234", files_ok=True)
        with patch.object(files, "exchange", return_value=reply):
            self.assertEqual(files.identify("unused"), reply)

    def test_serial_chunking_and_disconnect(self):
        _, data, lines = fixture()
        read_fd, write_fd = os.pipe()
        try:
            wire = b"\r\n".join(lines) + b"\r\n"
            os.write(write_fd, wire)
            os.close(write_fd)
            _, received = files.read_transfer(files.serial_lines(read_fd, time.monotonic() + 1), 7)
            self.assertEqual(received, data)
            with self.assertRaisesRegex(RuntimeError, "disconnected"):
                next(files.serial_lines(read_fd, time.monotonic() + 1))
        finally:
            os.close(read_fd)

    def test_fragmented_serial_lines(self):
        _, data, lines = fixture()
        wire = b"\r\n".join(lines) + b"\r\n"
        chunks = [wire[i:i + 7] for i in range(0, len(wire), 7)]
        with patch.object(files.select, "select", return_value=([9], [], [])), \
                patch.object(files.os, "read", side_effect=chunks):
            _, received = files.read_transfer(files.serial_lines(9, time.monotonic() + 1), 7)
        self.assertEqual(received, data)

    def test_timeout_and_wire_limit(self):
        with self.assertRaisesRegex(RuntimeError, "timed out"):
            next(files.serial_lines(9, time.monotonic() - 1))
        with patch.object(files.select, "select", return_value=([9], [], [])), \
                patch.object(files.os, "read", return_value=b"x" * 4096), \
                patch.object(files, "MAX_WIRE", 100):
            with self.assertRaisesRegex(ValueError, "transfer limit"):
                next(files.serial_lines(9, time.monotonic() + 1))

    def test_nonobject_reply(self):
        for reply in ([], None, True, "text"):
            with patch.object(files, "exchange", return_value=reply):
                for operation in (files.identify, files.list_files):
                    with self.subTest(reply=reply, operation=operation), self.assertRaises(ValueError):
                        operation("unused")


if __name__ == "__main__":
    unittest.main()
