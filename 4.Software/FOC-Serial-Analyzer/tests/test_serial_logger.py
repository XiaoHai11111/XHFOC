from __future__ import annotations

import json
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest import mock


PROJECT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_DIR))

import serial_logger  # noqa: E402


class FakeSerialPort:
    def __init__(self, stop_event: threading.Event, payloads: list[bytes]):
        self._stop_event = stop_event
        self._payloads = iter(payloads)
        self.closed = False
        self.writes: list[bytes] = []

    def read(self, _size: int) -> bytes:
        try:
            return next(self._payloads)
        except StopIteration:
            self._stop_event.set()
            return b""

    def close(self) -> None:
        self.closed = True

    def write(self, data: bytes) -> int:
        self.writes.append(bytes(data))
        return len(data)


class FakeSerialApi:
    def __init__(self, port: FakeSerialPort):
        self.port = port
        self.kwargs = None

    def Serial(self, **kwargs):  # noqa: N802 - mirrors pyserial API
        self.kwargs = kwargs
        return self.port


class SerialLoggerTests(unittest.TestCase):
    def test_metadata_write_falls_back_when_windows_replace_is_denied(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "metadata.json"
            with mock.patch.object(serial_logger.os, "replace", side_effect=PermissionError):
                serial_logger._write_json_atomic(path, {"status": "capturing"})

            self.assertEqual(
                json.loads(path.read_text(encoding="utf-8")),
                {"status": "capturing"},
            )
            self.assertFalse(path.with_name("metadata.json.tmp").exists())

    def test_project_config_is_valid_and_relative_output_is_resolved(self) -> None:
        config = serial_logger.load_config(PROJECT_DIR / "config.json")
        self.assertEqual(config["serial"]["port"], "COM8")
        self.assertEqual(config["serial"]["baudrate"], 921600)
        self.assertTrue(Path(config["capture"]["output_dir"]).is_absolute())

    def test_negative_duration_is_rejected(self) -> None:
        config = serial_logger._merge_dict(
            serial_logger.DEFAULT_CONFIG,
            {"capture": {"duration_s": -1}},
        )
        with self.assertRaises(serial_logger.ConfigError):
            serial_logger.validate_config(config)

    def test_scheduled_command_parser(self) -> None:
        self.assertEqual(
            serial_logger._parse_scheduled_command("2.5:!START"),
            {"at_s": 2.5, "text": "!START"},
        )
        with self.assertRaises(serial_logger.ConfigError):
            serial_logger._parse_scheduled_command("!START")

    def test_capture_preserves_raw_bytes_and_writes_metadata(self) -> None:
        stop_event = threading.Event()
        payloads = [b"boot\r\n", bytes.fromhex("0000807f")]
        fake_port = FakeSerialPort(stop_event, payloads)
        fake_api = FakeSerialApi(fake_port)

        with tempfile.TemporaryDirectory() as temp_dir:
            config = serial_logger._merge_dict(
                serial_logger.DEFAULT_CONFIG,
                {
                    "capture": {
                        "output_dir": temp_dir,
                        "duration_s": 0,
                        "flush_interval_s": 0,
                    }
                },
            )
            result = serial_logger.capture_serial(config, fake_api, lambda: [], stop_event)

            self.assertEqual(result.stop_reason, "user_stop")
            self.assertEqual(result.bytes_written, sum(map(len, payloads)))
            self.assertTrue(fake_port.closed)
            self.assertIsNotNone(result.session_dir)
            session_dir = result.session_dir
            assert session_dir is not None
            self.assertEqual((session_dir / "serial_raw.bin").read_bytes(), b"".join(payloads))

            records = [
                json.loads(line)
                for line in (session_dir / "chunks.jsonl").read_text(encoding="utf-8").splitlines()
            ]
            self.assertEqual([record["offset"] for record in records], [0, len(payloads[0])])
            metadata = json.loads((session_dir / "metadata.json").read_text(encoding="utf-8"))
            self.assertEqual(metadata["status"], "complete")
            self.assertEqual(metadata["bytes_written"], result.bytes_written)
            self.assertEqual(metadata["stop_reason"], "user_stop")
            self.assertEqual(fake_api.kwargs["port"], "COM8")

    def test_capture_sends_scheduled_and_final_commands(self) -> None:
        stop_event = threading.Event()
        fake_port = FakeSerialPort(stop_event, [b"ready\r\n"])
        fake_api = FakeSerialApi(fake_port)

        with tempfile.TemporaryDirectory() as temp_dir:
            config = serial_logger._merge_dict(
                serial_logger.DEFAULT_CONFIG,
                {
                    "capture": {
                        "output_dir": temp_dir,
                        "duration_s": 0,
                        "flush_interval_s": 0,
                        "commands": [{"at_s": 0, "text": "!START"}],
                        "final_command": "!STOP",
                    }
                },
            )
            result = serial_logger.capture_serial(config, fake_api, lambda: [], stop_event)

            self.assertEqual(fake_port.writes, [b"!START\r\n", b"!STOP\r\n"])
            assert result.session_dir is not None
            metadata = json.loads(
                (result.session_dir / "metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual([item["text"] for item in metadata["commands"]], ["!START", "!STOP"])
            self.assertFalse(metadata["commands"][0]["final"])
            self.assertTrue(metadata["commands"][1]["final"])


if __name__ == "__main__":
    unittest.main()
