#!/usr/bin/env python3
"""Configuration-driven serial byte-stream recorder for XHFOC HIL tests."""

from __future__ import annotations

import argparse
import copy
import ctypes
import json
import os
import signal
import sys
import threading
import time
import types
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable, Optional


DEFAULT_CONFIG: dict[str, Any] = {
    "experiment": {
        "name": "",
        "firmware_revision": "",
        "notes": "",
    },
    "serial": {
        "port": "COM8",
        "baudrate": 115200,
        "bytesize": 8,
        "parity": "N",
        "stopbits": 1,
        "read_timeout_s": 0.2,
        "xonxoff": False,
        "rtscts": False,
        "dsrdtr": False,
        "auto_reconnect": True,
        "reconnect_interval_s": 1.0,
        "connect_timeout_s": 0,
        "vid": None,
        "pid": None,
    },
    "capture": {
        "duration_s": 0,
        "output_dir": "captures",
        "file_prefix": "xhfoc",
        "read_size": 4096,
        "flush_interval_s": 1.0,
        "fsync": True,
        "commands": [],
        "final_command": "",
    },
}

DEFAULT_CONFIG_PATH = Path(__file__).resolve().with_name("config.json")
_WINDOWS_HANDLER_REFERENCES: list[Any] = []


class ConfigError(ValueError):
    """Raised when the configuration is missing or invalid."""


@dataclass(frozen=True)
class CaptureResult:
    session_dir: Optional[Path]
    bytes_written: int
    chunks_written: int
    stop_reason: str


def _merge_dict(base: dict[str, Any], override: dict[str, Any]) -> dict[str, Any]:
    result = copy.deepcopy(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = _merge_dict(result[key], value)
        else:
            result[key] = value
    return result


def _require_number(value: Any, name: str, minimum: float) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ConfigError(f"{name} 必须是数字")
    number = float(value)
    if number < minimum:
        raise ConfigError(f"{name} 不能小于 {minimum}")
    return number


def validate_config(config: dict[str, Any]) -> None:
    experiment_cfg = config.get("experiment")
    serial_cfg = config.get("serial")
    capture_cfg = config.get("capture")
    if not isinstance(experiment_cfg, dict) or not isinstance(serial_cfg, dict) or not isinstance(capture_cfg, dict):
        raise ConfigError("配置必须包含 experiment、serial 和 capture 对象")
    for key in ("name", "firmware_revision", "notes"):
        if not isinstance(experiment_cfg.get(key), str):
            raise ConfigError(f"experiment.{key} 必须是字符串")

    port = serial_cfg.get("port")
    if not isinstance(port, str) or not port.strip():
        raise ConfigError("serial.port 必须是非空字符串，可使用 COM8 或 auto")

    baudrate = serial_cfg.get("baudrate")
    if isinstance(baudrate, bool) or not isinstance(baudrate, int) or baudrate <= 0:
        raise ConfigError("serial.baudrate 必须是正整数")
    if serial_cfg.get("bytesize") not in (5, 6, 7, 8):
        raise ConfigError("serial.bytesize 只能是 5、6、7 或 8")
    if str(serial_cfg.get("parity", "")).upper() not in ("N", "E", "O", "M", "S"):
        raise ConfigError("serial.parity 只能是 N、E、O、M 或 S")
    if serial_cfg.get("stopbits") not in (1, 1.5, 2):
        raise ConfigError("serial.stopbits 只能是 1、1.5 或 2")

    _require_number(serial_cfg.get("read_timeout_s"), "serial.read_timeout_s", 0.01)
    _require_number(serial_cfg.get("reconnect_interval_s"), "serial.reconnect_interval_s", 0)
    _require_number(serial_cfg.get("connect_timeout_s"), "serial.connect_timeout_s", 0)
    for key in ("xonxoff", "rtscts", "dsrdtr", "auto_reconnect"):
        if not isinstance(serial_cfg.get(key), bool):
            raise ConfigError(f"serial.{key} 必须是 true 或 false")
    for key in ("vid", "pid"):
        value = serial_cfg.get(key)
        if value is not None and (isinstance(value, bool) or not isinstance(value, int) or value < 0):
            raise ConfigError(f"serial.{key} 必须是非负整数或 null")

    _require_number(capture_cfg.get("duration_s"), "capture.duration_s", 0)
    output_dir = capture_cfg.get("output_dir")
    if not isinstance(output_dir, str) or not output_dir.strip():
        raise ConfigError("capture.output_dir 必须是非空字符串")
    prefix = capture_cfg.get("file_prefix")
    if not isinstance(prefix, str) or not prefix.strip() or any(c in prefix for c in '<>:"/\\|?*'):
        raise ConfigError("capture.file_prefix 不能为空且不能包含路径非法字符")
    read_size = capture_cfg.get("read_size")
    if isinstance(read_size, bool) or not isinstance(read_size, int) or read_size <= 0:
        raise ConfigError("capture.read_size 必须是正整数")
    _require_number(capture_cfg.get("flush_interval_s"), "capture.flush_interval_s", 0)
    if not isinstance(capture_cfg.get("fsync"), bool):
        raise ConfigError("capture.fsync 必须是 true 或 false")
    commands = capture_cfg.get("commands")
    if not isinstance(commands, list):
        raise ConfigError("capture.commands 必须是数组")
    for index, command in enumerate(commands):
        if not isinstance(command, dict):
            raise ConfigError(f"capture.commands[{index}] 必须是对象")
        _require_number(command.get("at_s"), f"capture.commands[{index}].at_s", 0)
        text = command.get("text")
        if not isinstance(text, str) or not text.strip() or len(text) > 128:
            raise ConfigError(f"capture.commands[{index}].text 必须是 1~128 字符的非空字符串")
        try:
            text.encode("ascii")
        except UnicodeEncodeError as exc:
            raise ConfigError(f"capture.commands[{index}].text 当前只支持 ASCII") from exc
    final_command = capture_cfg.get("final_command")
    if not isinstance(final_command, str) or len(final_command) > 128:
        raise ConfigError("capture.final_command 必须是不超过 128 字符的字符串")
    try:
        final_command.encode("ascii")
    except UnicodeEncodeError as exc:
        raise ConfigError("capture.final_command 当前只支持 ASCII") from exc


def load_config(path: Path) -> dict[str, Any]:
    config_path = path.expanduser().resolve()
    try:
        parsed = json.loads(config_path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ConfigError(f"找不到配置文件: {config_path}") from exc
    except json.JSONDecodeError as exc:
        raise ConfigError(f"配置文件 JSON 无效（第 {exc.lineno} 行）: {exc.msg}") from exc
    if not isinstance(parsed, dict):
        raise ConfigError("配置文件根节点必须是 JSON 对象")

    config = _merge_dict(DEFAULT_CONFIG, parsed)
    validate_config(config)
    output_dir = Path(config["capture"]["output_dir"]).expanduser()
    if not output_dir.is_absolute():
        output_dir = config_path.parent / output_dir
    config["capture"]["output_dir"] = str(output_dir.resolve())
    return config


def _utc_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def _safe_close(serial_port: Any) -> None:
    if serial_port is None:
        return
    try:
        serial_port.close()
    except Exception:
        pass


def _write_json_atomic(path: Path, payload: dict[str, Any]) -> None:
    content = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(content, encoding="utf-8")
    try:
        os.replace(temporary, path)
    except PermissionError:
        # Some Windows antivirus/indexing filters briefly lock metadata.json
        # while the capture is active. A direct rewrite is preferable to
        # aborting the serial session (the raw stream remains authoritative).
        path.write_text(content, encoding="utf-8")
        try:
            temporary.unlink()
        except FileNotFoundError:
            pass


class CaptureWriter:
    """Persist raw bytes, a chunk time index, and session metadata."""

    def __init__(self, config: dict[str, Any], port: str, started_monotonic: float):
        capture_cfg = config["capture"]
        output_root = Path(capture_cfg["output_dir"])
        output_root.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().astimezone().strftime("%Y%m%d_%H%M%S_%f")
        self.session_dir = output_root / f"{capture_cfg['file_prefix']}_{stamp}"
        self.session_dir.mkdir(parents=False, exist_ok=False)

        self.raw_path = self.session_dir / "serial_raw.bin"
        self.index_path = self.session_dir / "chunks.jsonl"
        self.metadata_path = self.session_dir / "metadata.json"
        self._raw = self.raw_path.open("xb")
        self._index = self.index_path.open("x", encoding="utf-8", newline="\n")
        self._started_monotonic = started_monotonic
        self._last_flush = started_monotonic
        self.bytes_written = 0
        self.chunks_written = 0
        self._fsync_enabled = bool(capture_cfg["fsync"])
        self._flush_interval_s = float(capture_cfg["flush_interval_s"])
        self.metadata: dict[str, Any] = {
            "schema_version": 1,
            "status": "capturing",
            "started_at": _utc_iso(),
            "finished_at": None,
            "stop_reason": None,
            "selected_port": port,
            "bytes_written": 0,
            "chunks_written": 0,
            "files": {
                "raw": self.raw_path.name,
                "chunk_index": self.index_path.name,
            },
            "experiment": copy.deepcopy(config["experiment"]),
            "serial": copy.deepcopy(config["serial"]),
            "capture": copy.deepcopy(config["capture"]),
            "connections": [],
            "commands": [],
            "errors": [],
        }
        _write_json_atomic(self.metadata_path, self.metadata)

    def connection_opened(self, port: str) -> None:
        self.metadata["connections"].append(
            {"port": port, "opened_at": _utc_iso(), "closed_at": None, "close_reason": None}
        )
        _write_json_atomic(self.metadata_path, self.metadata)

    def connection_closed(self, reason: str) -> None:
        if self.metadata["connections"]:
            connection = self.metadata["connections"][-1]
            if connection["closed_at"] is None:
                connection["closed_at"] = _utc_iso()
                connection["close_reason"] = reason

    def add_error(self, message: str) -> None:
        self.metadata["errors"].append({"at": _utc_iso(), "message": message})
        _write_json_atomic(self.metadata_path, self.metadata)

    def command_sent(
        self,
        text: str,
        sent_monotonic: float,
        scheduled_at_s: Optional[float],
        final: bool = False,
    ) -> None:
        self.metadata["commands"].append(
            {
                "text": text.rstrip("\r\n"),
                "scheduled_at_s": scheduled_at_s,
                "sent_elapsed_s": round(sent_monotonic - self._started_monotonic, 6),
                "sent_at": _utc_iso(),
                "final": final,
            }
        )
        _write_json_atomic(self.metadata_path, self.metadata)

    def write(self, data: bytes, now_monotonic: float) -> None:
        offset = self.bytes_written
        self._raw.write(data)
        record = {
            "chunk": self.chunks_written,
            "received_at": _utc_iso(),
            "elapsed_s": round(now_monotonic - self._started_monotonic, 6),
            "offset": offset,
            "length": len(data),
        }
        self._index.write(json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n")
        self.bytes_written += len(data)
        self.chunks_written += 1
        if now_monotonic - self._last_flush >= self._flush_interval_s:
            self.flush()
            self._last_flush = now_monotonic

    def flush(self) -> None:
        self._raw.flush()
        self._index.flush()
        if self._fsync_enabled:
            os.fsync(self._raw.fileno())
            os.fsync(self._index.fileno())

    def close(self, stop_reason: str) -> None:
        self.connection_closed(stop_reason)
        try:
            self.flush()
        finally:
            self._raw.close()
            self._index.close()
        self.metadata.update(
            {
                "status": "complete",
                "finished_at": _utc_iso(),
                "stop_reason": stop_reason,
                "bytes_written": self.bytes_written,
                "chunks_written": self.chunks_written,
            }
        )
        _write_json_atomic(self.metadata_path, self.metadata)


def _port_matches(device: Any, vid: Optional[int], pid: Optional[int]) -> bool:
    return (vid is None or getattr(device, "vid", None) == vid) and (
        pid is None or getattr(device, "pid", None) == pid
    )


def select_port(serial_cfg: dict[str, Any], list_ports: Callable[[], Iterable[Any]]) -> str:
    requested = serial_cfg["port"].strip()
    if requested.lower() != "auto":
        return requested
    candidates = [
        device
        for device in list_ports()
        if _port_matches(device, serial_cfg.get("vid"), serial_cfg.get("pid"))
    ]
    if not candidates:
        raise OSError("未找到匹配的串口")
    candidates.sort(key=lambda item: str(getattr(item, "device", "")))
    if len(candidates) > 1:
        names = ", ".join(str(item.device) for item in candidates)
        print(f"[WARN] auto 匹配到多个串口（{names}），选择 {candidates[0].device}")
    return str(candidates[0].device)


def _open_serial(serial_api: Any, serial_cfg: dict[str, Any], port: str) -> Any:
    return serial_api.Serial(
        port=port,
        baudrate=serial_cfg["baudrate"],
        bytesize=serial_cfg["bytesize"],
        parity=serial_cfg["parity"].upper(),
        stopbits=serial_cfg["stopbits"],
        timeout=float(serial_cfg["read_timeout_s"]),
        xonxoff=serial_cfg["xonxoff"],
        rtscts=serial_cfg["rtscts"],
        dsrdtr=serial_cfg["dsrdtr"],
    )


def _command_payload(text: str) -> bytes:
    return (text.rstrip("\r\n") + "\r\n").encode("ascii")


def _write_command(serial_port: Any, text: str) -> int:
    payload = _command_payload(text)
    total = 0
    while total < len(payload):
        written = serial_port.write(payload[total:])
        if written is None:
            written = len(payload) - total
        if not isinstance(written, int) or written <= 0:
            raise OSError("串口命令写入未取得进展")
        total += written
    flush = getattr(serial_port, "flush", None)
    if callable(flush):
        flush()
    return total


def capture_serial(
    config: dict[str, Any],
    serial_api: Any,
    list_ports: Callable[[], Iterable[Any]],
    stop_event: Optional[threading.Event] = None,
) -> CaptureResult:
    """Capture until duration expires or stop_event is set."""
    validate_config(config)
    event = stop_event or threading.Event()
    serial_cfg = config["serial"]
    capture_cfg = config["capture"]
    process_started = time.monotonic()
    first_connected: Optional[float] = None
    deadline: Optional[float] = None
    serial_port: Any = None
    writer: Optional[CaptureWriter] = None
    stop_reason = "stop_requested"
    last_connect_error: Optional[str] = None
    command_schedule = sorted(
        (copy.deepcopy(item) for item in capture_cfg["commands"]),
        key=lambda item: float(item["at_s"]),
    )
    next_command = 0
    final_command = str(capture_cfg["final_command"]).strip()

    try:
        while not event.is_set():
            now = time.monotonic()
            if deadline is not None and now >= deadline:
                stop_reason = "duration_elapsed"
                break

            if serial_port is None:
                connect_timeout = float(serial_cfg["connect_timeout_s"])
                if connect_timeout > 0 and now - process_started >= connect_timeout:
                    stop_reason = "connect_timeout"
                    message = last_connect_error or "连接串口超时"
                    if writer is not None:
                        writer.add_error(message)
                        break
                    raise RuntimeError(message)
                try:
                    selected_port = select_port(serial_cfg, list_ports)
                    opened_port = _open_serial(serial_api, serial_cfg, selected_port)
                except Exception as exc:
                    last_connect_error = f"串口连接失败: {exc}"
                    print(f"[WARN] {last_connect_error}")
                    if not serial_cfg["auto_reconnect"]:
                        raise RuntimeError(last_connect_error) from exc
                    event.wait(float(serial_cfg["reconnect_interval_s"]))
                    continue

                now = time.monotonic()
                if first_connected is None:
                    first_connected = now
                    duration = float(capture_cfg["duration_s"])
                    deadline = first_connected + duration if duration > 0 else None
                    try:
                        writer = CaptureWriter(config, selected_port, first_connected)
                    except Exception:
                        _safe_close(opened_port)
                        raise
                serial_port = opened_port
                assert writer is not None
                writer.connection_opened(selected_port)
                last_connect_error = None
                mode = "持续采集" if deadline is None else f"采集 {capture_cfg['duration_s']} 秒"
                framing = (
                    f"{serial_cfg['baudrate']}/{serial_cfg['bytesize']}"
                    f"{serial_cfg['parity'].upper()}{serial_cfg['stopbits']}"
                )
                print(f"[INFO] 已连接 {selected_port}，{framing}，{mode}")
                print(f"[INFO] 会话目录: {writer.session_dir}")

            command_failed = False
            while next_command < len(command_schedule) and first_connected is not None:
                command = command_schedule[next_command]
                now = time.monotonic()
                elapsed = now - first_connected
                scheduled_at_s = float(command["at_s"])
                if elapsed < scheduled_at_s:
                    break
                text = str(command["text"]).strip()
                try:
                    _write_command(serial_port, text)
                except Exception as exc:
                    message = f"串口命令发送失败（{text}）: {exc}"
                    print(f"[ERROR] {message}")
                    assert writer is not None
                    writer.add_error(message)
                    stop_reason = "command_error"
                    command_failed = True
                    break
                assert writer is not None
                writer.command_sent(text, now, scheduled_at_s)
                print(f"[INFO] 已在 {elapsed:.3f}s 发送命令: {text}")
                next_command += 1
            if command_failed:
                break

            try:
                data = serial_port.read(capture_cfg["read_size"])
            except Exception as exc:
                message = f"串口读取中断: {exc}"
                print(f"[WARN] {message}")
                if writer is not None:
                    writer.connection_closed("read_error")
                    writer.add_error(message)
                _safe_close(serial_port)
                serial_port = None
                if not serial_cfg["auto_reconnect"]:
                    stop_reason = "read_error"
                    break
                continue
            if data:
                assert writer is not None
                writer.write(bytes(data), time.monotonic())

        if event.is_set() and stop_reason == "stop_requested":
            stop_reason = "user_stop"
    finally:
        if serial_port is not None and final_command:
            try:
                now = time.monotonic()
                _write_command(serial_port, final_command)
                if writer is not None:
                    writer.command_sent(final_command, now, None, final=True)
                print(f"[INFO] 收尾命令已发送: {final_command}")
                time.sleep(0.05)
            except Exception as exc:
                message = f"收尾命令发送失败（{final_command}）: {exc}"
                print(f"[WARN] {message}")
                if writer is not None:
                    writer.add_error(message)
        _safe_close(serial_port)
        if writer is not None:
            writer.close(stop_reason)

    return CaptureResult(
        session_dir=writer.session_dir if writer else None,
        bytes_written=writer.bytes_written if writer else 0,
        chunks_written=writer.chunks_written if writer else 0,
        stop_reason=stop_reason,
    )


def _load_pyserial() -> tuple[Any, Callable[[], Iterable[Any]]]:
    try:
        import serial  # type: ignore
        from serial.tools import list_ports  # type: ignore
    except ModuleNotFoundError as exc:
        if os.name == "nt":
            try:
                from windows_serial import WindowsSerial, comports
            except (ImportError, OSError) as native_exc:
                raise RuntimeError(f"无法加载 Windows 串口后端: {native_exc}") from native_exc
            print("[INFO] 使用内置 Win32 串口后端（Windows 下无需安装 pyserial）")
            return types.SimpleNamespace(Serial=WindowsSerial), comports
        raise RuntimeError("缺少 pyserial，请先执行: python -m pip install -r requirements.txt") from exc
    return serial, list_ports.comports


def install_stop_handlers(stop_event: threading.Event) -> None:
    def handle_signal(signum: int, _frame: Any) -> None:
        print(f"\n[INFO] 收到停止信号 {signum}，正在关闭串口并保存数据...")
        stop_event.set()

    signal.signal(signal.SIGINT, handle_signal)
    if hasattr(signal, "SIGBREAK"):
        signal.signal(signal.SIGBREAK, handle_signal)
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, handle_signal)

    if os.name == "nt":
        handler_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_uint)

        @handler_type
        def windows_console_handler(control_type: int) -> bool:
            if control_type in (0, 1, 2, 5, 6):
                stop_event.set()
                return True
            return False

        if ctypes.windll.kernel32.SetConsoleCtrlHandler(windows_console_handler, True):
            _WINDOWS_HANDLER_REFERENCES.append(windows_console_handler)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="XHFOC 配置驱动串口数据采集器")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG_PATH, help="JSON 配置文件")
    parser.add_argument("--port", help="临时覆盖 serial.port，例如 COM8 或 auto")
    parser.add_argument("--duration", type=float, help="临时覆盖采集秒数；0 表示持续采集")
    parser.add_argument("--output-dir", type=Path, help="临时覆盖输出目录")
    parser.add_argument(
        "--send",
        action="append",
        metavar="SECONDS:COMMAND",
        help="连接后定时发送 ASCII 命令，可重复，例如 2:!START",
    )
    parser.add_argument("--final-command", help="退出前发送的收尾命令，例如 !STOP")
    parser.add_argument("--check-config", action="store_true", help="校验并打印最终配置后退出")
    parser.add_argument("--list-ports", action="store_true", help="列出当前串口后退出")
    return parser


def _parse_scheduled_command(value: str) -> dict[str, Any]:
    delay_text, separator, command = value.partition(":")
    if not separator or not command.strip():
        raise ConfigError("--send 格式必须为 SECONDS:COMMAND，例如 2:!START")
    try:
        delay = float(delay_text)
    except ValueError as exc:
        raise ConfigError("--send 的 SECONDS 必须是数字") from exc
    if delay < 0:
        raise ConfigError("--send 的 SECONDS 不能为负数")
    return {"at_s": delay, "text": command.strip()}


def main(argv: Optional[list[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        config = load_config(args.config)
        if args.port:
            config["serial"]["port"] = args.port
        if args.duration is not None:
            config["capture"]["duration_s"] = args.duration
        if args.output_dir:
            config["capture"]["output_dir"] = str(args.output_dir.expanduser().resolve())
        if args.send:
            config["capture"]["commands"] = [_parse_scheduled_command(item) for item in args.send]
        if args.final_command is not None:
            config["capture"]["final_command"] = args.final_command
        validate_config(config)

        if args.check_config:
            print(json.dumps(config, ensure_ascii=False, indent=2))
            return 0

        serial_api, list_ports = _load_pyserial()
        if args.list_ports:
            devices = list(list_ports())
            if not devices:
                print("未发现串口")
            for device in devices:
                print(f"{device.device}\t{getattr(device, 'description', '')}\t{getattr(device, 'hwid', '')}")
            return 0

        stop_event = threading.Event()
        install_stop_handlers(stop_event)
        result = capture_serial(config, serial_api, list_ports, stop_event)
        if result.session_dir is None:
            print(f"[INFO] 未建立采集会话，停止原因: {result.stop_reason}")
        else:
            print(
                f"[INFO] 已关闭串口并保存 {result.bytes_written} 字节/{result.chunks_written} 块，"
                f"停止原因: {result.stop_reason}"
            )
            print(f"[INFO] 数据目录: {result.session_dir}")
        return 0
    except (ConfigError, RuntimeError, OSError) as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
