#!/usr/bin/env python3
"""Decode an XHFOC capture into VOFA CSV, ASCII log, and a JSON summary."""

from __future__ import annotations

import argparse
import bisect
import csv
import json
import math
import re
import struct
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_DECODER_CONFIG = SCRIPT_DIR / "decoder_config.json"
TAIL_PATTERN = re.compile(rb"[\x20-\x7e\t]{4,}")
TAGGED_LOG_PATTERN = re.compile(r"^\[[A-Za-z0-9_.:-]{1,24}\](?:\s|$)")
COMMAND_RESPONSE_PREFIXES = (
    "Stopped ok",
    "Started ok",
    "Start rejected:",
    "DISABLE ignored",
)


class DecodeError(RuntimeError):
    """Raised when a capture cannot be decoded safely."""


@dataclass(frozen=True)
class DecoderConfig:
    channels: tuple[str, ...]
    tail: bytes
    struct_format: str
    expected_baudrate: Optional[int] = None

    @property
    def payload_size(self) -> int:
        return len(self.channels) * 4

    @property
    def frame_size(self) -> int:
        return self.payload_size + len(self.tail)


@dataclass(frozen=True)
class ChunkTime:
    offset: int
    end: int
    elapsed_s: float
    received_at: str


@dataclass(frozen=True)
class DecodedFrame:
    index: int
    offset: int
    elapsed_s: Optional[float]
    received_at: str
    values: tuple[float, ...]


@dataclass(frozen=True)
class DecodeResult:
    frames: tuple[DecodedFrame, ...]
    gaps: tuple[tuple[int, bytes], ...]
    tail_markers: int
    rejected_markers: int
    nonfinite_frames: int


def load_decoder_config(path: Path) -> DecoderConfig:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise DecodeError(f"找不到解析配置: {path}") from exc
    except json.JSONDecodeError as exc:
        raise DecodeError(f"解析配置 JSON 无效（第 {exc.lineno} 行）: {exc.msg}") from exc

    if not isinstance(payload, dict):
        raise DecodeError("解析配置根节点必须是 JSON 对象")
    if payload.get("format") != "vofa-justfloat":
        raise DecodeError("当前只支持 format=vofa-justfloat")
    channels = payload.get("channels")
    if not isinstance(channels, list) or not channels or not all(isinstance(item, str) and item for item in channels):
        raise DecodeError("channels 必须是非空字符串数组")
    if len(set(channels)) != len(channels):
        raise DecodeError("channels 不能包含重复名称")

    try:
        tail = bytes.fromhex(payload["frame_tail_hex"])
    except (KeyError, TypeError, ValueError) as exc:
        raise DecodeError("frame_tail_hex 必须是有效的十六进制字符串") from exc
    if not tail:
        raise DecodeError("frame_tail_hex 不能为空")
    byte_order = payload.get("endianness")
    if byte_order not in ("little", "big"):
        raise DecodeError("endianness 只能是 little 或 big")
    expected_baudrate = payload.get("expected_baudrate")
    if expected_baudrate is not None and (
        isinstance(expected_baudrate, bool)
        or not isinstance(expected_baudrate, int)
        or expected_baudrate <= 0
    ):
        raise DecodeError("expected_baudrate 必须是正整数")
    prefix = "<" if byte_order == "little" else ">"
    return DecoderConfig(
        tuple(channels),
        tail,
        prefix + f"{len(channels)}f",
        expected_baudrate,
    )


def load_chunks(path: Path) -> list[ChunkTime]:
    if not path.exists():
        return []
    chunks: list[ChunkTime] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip():
            continue
        try:
            item = json.loads(line)
            offset = int(item["offset"])
            length = int(item["length"])
            elapsed_s = float(item["elapsed_s"])
            received_at = str(item.get("received_at", ""))
        except (KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
            raise DecodeError(f"chunks.jsonl 第 {line_number} 行无效") from exc
        if offset < 0 or length < 0:
            raise DecodeError(f"chunks.jsonl 第 {line_number} 行包含负偏移或长度")
        chunks.append(ChunkTime(offset, offset + length, elapsed_s, received_at))
    chunks.sort(key=lambda item: item.offset)
    return chunks


class TimeMapper:
    def __init__(self, chunks: list[ChunkTime]):
        self._chunks = chunks
        self._starts = [item.offset for item in chunks]

    def lookup(self, offset: int) -> tuple[Optional[float], str]:
        if not self._chunks:
            return None, ""
        index = bisect.bisect_right(self._starts, offset) - 1
        if index < 0:
            return None, ""
        chunk = self._chunks[index]
        if offset >= chunk.end:
            return None, ""
        return chunk.elapsed_s, chunk.received_at


def _find_all(data: bytes, marker: bytes) -> list[int]:
    positions: list[int] = []
    cursor = 0
    while True:
        position = data.find(marker, cursor)
        if position < 0:
            return positions
        positions.append(position)
        cursor = position + 1


def decode_raw(data: bytes, config: DecoderConfig, chunks: list[ChunkTime]) -> DecodeResult:
    markers = _find_all(data, config.tail)
    mapper = TimeMapper(chunks)
    frames: list[DecodedFrame] = []
    gaps: list[tuple[int, bytes]] = []
    accepted_end = 0
    rejected = 0
    nonfinite_frames = 0

    for marker_index, marker_offset in enumerate(markers):
        next_marker = markers[marker_index + 1] if marker_index + 1 < len(markers) else None
        if next_marker is not None and next_marker - marker_offset < config.frame_size:
            rejected += 1
            continue
        frame_start = marker_offset - config.payload_size
        frame_end = marker_offset + len(config.tail)
        if frame_start < accepted_end or frame_start < 0:
            rejected += 1
            continue
        payload = data[frame_start:marker_offset]
        if len(payload) != config.payload_size:
            rejected += 1
            continue
        try:
            values = struct.unpack(config.struct_format, payload)
        except struct.error:
            rejected += 1
            continue

        if frame_start > accepted_end:
            gaps.append((accepted_end, data[accepted_end:frame_start]))
        elapsed_s, received_at = mapper.lookup(frame_end - 1)
        if not all(math.isfinite(value) for value in values):
            nonfinite_frames += 1
        frames.append(
            DecodedFrame(
                index=len(frames),
                offset=frame_start,
                elapsed_s=elapsed_s,
                received_at=received_at,
                values=tuple(values),
            )
        )
        accepted_end = frame_end

    if accepted_end < len(data):
        gaps.append((accepted_end, data[accepted_end:]))
    return DecodeResult(tuple(frames), tuple(gaps), len(markers), rejected, nonfinite_frames)


def extract_ascii(gaps: tuple[tuple[int, bytes], ...]) -> list[tuple[int, str]]:
    records: list[tuple[int, str]] = []
    for gap_offset, gap in gaps:
        for match in TAIL_PATTERN.finditer(gap):
            text = match.group().decode("ascii", errors="strict").strip()
            if text and (
                TAGGED_LOG_PATTERN.match(text)
                or text.startswith(COMMAND_RESPONSE_PREFIXES)
            ):
                records.append((gap_offset + match.start(), text))
    return records


def _load_metadata(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    try:
        metadata = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise DecodeError(f"metadata.json 无效（第 {exc.lineno} 行）") from exc
    return metadata if isinstance(metadata, dict) else {}


def _latest_capture(captures_dir: Path) -> Path:
    candidates = [item.parent for item in captures_dir.glob("*/serial_raw.bin")]
    if not candidates:
        raise DecodeError(f"未在 {captures_dir} 中找到采集会话")
    complete: list[Path] = []
    for candidate in candidates:
        metadata = _load_metadata(candidate / "metadata.json")
        if metadata.get("status") == "complete":
            complete.append(candidate)
    pool = complete or candidates
    return max(pool, key=lambda item: (item / "serial_raw.bin").stat().st_mtime)


def resolve_capture(path: Optional[Path]) -> tuple[Path, Path]:
    target = path.expanduser().resolve() if path else _latest_capture(SCRIPT_DIR / "captures")
    if target.is_dir():
        raw_path = target / "serial_raw.bin"
        session_dir = target
    else:
        raw_path = target
        session_dir = target.parent
    if not raw_path.exists():
        raise DecodeError(f"找不到原始串口数据: {raw_path}")
    return session_dir, raw_path


def write_outputs(
    session_dir: Path,
    raw_path: Path,
    output_dir: Path,
    config: DecoderConfig,
    result: DecodeResult,
    metadata: dict[str, Any],
) -> dict[str, Any]:
    output_dir.mkdir(parents=True, exist_ok=True)
    csv_path = output_dir / "vofa.csv"
    ascii_path = output_dir / "ascii.log"
    summary_path = output_dir / "parse_summary.json"

    with csv_path.open("w", encoding="utf-8-sig", newline="") as output:
        writer = csv.writer(output)
        writer.writerow(
            ["frame_index", "elapsed_s", "delta_ms", "received_at", "source_offset", *config.channels]
        )
        previous_elapsed: Optional[float] = None
        for frame in result.frames:
            delta_ms: Any = ""
            if frame.elapsed_s is not None and previous_elapsed is not None:
                delta_ms = f"{(frame.elapsed_s - previous_elapsed) * 1000:.6f}"
            elapsed: Any = "" if frame.elapsed_s is None else f"{frame.elapsed_s:.6f}"
            writer.writerow(
                [
                    frame.index,
                    elapsed,
                    delta_ms,
                    frame.received_at,
                    frame.offset,
                    *(format(value, ".9g") for value in frame.values),
                ]
            )
            if frame.elapsed_s is not None:
                previous_elapsed = frame.elapsed_s

    ascii_records = extract_ascii(result.gaps)
    with ascii_path.open("w", encoding="utf-8", newline="\n") as output:
        for offset, text in ascii_records:
            output.write(f"[offset={offset}] {text}\n")

    timed_frames = [frame for frame in result.frames if frame.elapsed_s is not None]
    duration_s: Optional[float] = None
    estimated_rate_hz: Optional[float] = None
    if len(timed_frames) >= 2:
        duration_s = timed_frames[-1].elapsed_s - timed_frames[0].elapsed_s  # type: ignore[operator]
        if duration_s > 0:
            estimated_rate_hz = (len(timed_frames) - 1) / duration_s

    non_frame_bytes = sum(len(data) for _, data in result.gaps)
    metadata_bytes = metadata.get("bytes_written")
    metadata_serial = metadata.get("serial")
    captured_baudrate = (
        metadata_serial.get("baudrate")
        if isinstance(metadata_serial, dict)
        else None
    )
    raw_size = raw_path.stat().st_size
    summary: dict[str, Any] = {
        "schema_version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
        "source": {
            "session_dir": str(session_dir),
            "raw_file": raw_path.name,
            "raw_bytes": raw_size,
            "capture_status": metadata.get("status"),
            "capture_stop_reason": metadata.get("stop_reason"),
            "metadata_bytes_written": metadata_bytes,
            "raw_size_matches_metadata": metadata_bytes == raw_size if isinstance(metadata_bytes, int) else None,
            "expected_baudrate": config.expected_baudrate,
            "captured_baudrate": captured_baudrate,
            "baudrate_matches_config": (
                captured_baudrate == config.expected_baudrate
                if isinstance(captured_baudrate, int)
                and isinstance(config.expected_baudrate, int)
                else None
            ),
        },
        "format": {
            "name": "vofa-justfloat",
            "channels": list(config.channels),
            "channel_count": len(config.channels),
            "frame_size": config.frame_size,
            "frame_tail_hex": config.tail.hex(),
        },
        "result": {
            "frames_decoded": len(result.frames),
            "tail_markers_found": result.tail_markers,
            "tail_markers_rejected": result.rejected_markers,
            "nonfinite_frames": result.nonfinite_frames,
            "non_frame_bytes": non_frame_bytes,
            "ascii_fragments": len(ascii_records),
            "timed_frames": len(timed_frames),
            "timed_duration_s": duration_s,
            "estimated_frame_rate_hz": estimated_rate_hz,
        },
        "outputs": {
            "csv": csv_path.name,
            "ascii_log": ascii_path.name,
        },
    }
    summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return summary


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="解析 XHFOC VOFA JustFloat 串口采集")
    parser.add_argument("capture", nargs="?", type=Path, help="采集会话目录或 serial_raw.bin；默认选最新完整会话")
    parser.add_argument("--config", type=Path, default=DEFAULT_DECODER_CONFIG, help="解析格式和通道配置")
    parser.add_argument("--output-dir", type=Path, help="输出目录；默认为会话目录下 decoded")
    parser.add_argument("--allow-incomplete", action="store_true", help="允许解析仍在采集或未正常结束的会话快照")
    return parser


def main(argv: Optional[list[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        session_dir, raw_path = resolve_capture(args.capture)
        metadata = _load_metadata(session_dir / "metadata.json")
        if metadata.get("status") not in (None, "complete") and not args.allow_incomplete:
            raise DecodeError(
                f"会话状态为 {metadata.get('status')!r}，请先正常停止采集，或使用 --allow-incomplete 解析快照"
            )
        config = load_decoder_config(args.config.expanduser().resolve())
        chunks = load_chunks(session_dir / "chunks.jsonl")
        data = raw_path.read_bytes()
        result = decode_raw(data, config, chunks)
        output_dir = args.output_dir.expanduser().resolve() if args.output_dir else session_dir / "decoded"
        summary = write_outputs(session_dir, raw_path, output_dir, config, result, metadata)
        stats = summary["result"]
        print(f"[INFO] 已解析 {stats['frames_decoded']} 帧，非帧字节 {stats['non_frame_bytes']}")
        print(f"[INFO] CSV: {output_dir / 'vofa.csv'}")
        print(f"[INFO] 文本日志: {output_dir / 'ascii.log'}")
        print(f"[INFO] 摘要: {output_dir / 'parse_summary.json'}")
        return 0
    except (DecodeError, OSError) as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
