from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_DIR))

from decode_capture import ChunkTime, DecoderConfig, decode_raw, extract_ascii  # noqa: E402


class DecodeCaptureTests(unittest.TestCase):
    def test_mixed_ascii_and_vofa_frames_are_separated(self) -> None:
        config = DecoderConfig(("a", "b"), bytes.fromhex("0000807f"), "<2f")
        frame1 = struct.pack("<2f", 1.25, -2.5) + config.tail
        frame2 = struct.pack("<2f", 3.0, 4.5) + config.tail
        prefix = b"[sys] ready\r\n"
        middle = b"[key] start\r\n"
        suffix = b"Started ok\r\n"
        raw = prefix + frame1 + middle + frame2 + suffix
        chunks = [ChunkTime(0, len(raw), 0.125, "2026-07-17T00:00:00.125Z")]

        result = decode_raw(raw, config, chunks)

        self.assertEqual(len(result.frames), 2)
        self.assertEqual(result.frames[0].values, (1.25, -2.5))
        self.assertEqual(result.frames[1].values, (3.0, 4.5))
        self.assertEqual(result.frames[0].elapsed_s, 0.125)
        text = [item[1] for item in extract_ascii(result.gaps)]
        self.assertIn("[sys] ready", text)
        self.assertIn("[key] start", text)
        self.assertIn("Started ok", text)

    def test_truncated_prefix_before_first_tail_is_not_decoded_as_frame(self) -> None:
        config = DecoderConfig(("a", "b"), bytes.fromhex("0000807f"), "<2f")
        complete = struct.pack("<2f", 8.0, 9.0) + config.tail
        raw = b"\x00\x00" + config.tail + complete

        result = decode_raw(raw, config, [])

        self.assertEqual(len(result.frames), 1)
        self.assertEqual(result.frames[0].values, (8.0, 9.0))
        self.assertGreaterEqual(result.rejected_markers, 1)


if __name__ == "__main__":
    unittest.main()
