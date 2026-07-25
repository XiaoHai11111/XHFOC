"""Minimal standard-library Win32 serial backend used when pyserial is absent."""

from __future__ import annotations

import ctypes
import os
import re
from ctypes import wintypes
from dataclasses import dataclass
from typing import Optional


if os.name != "nt":
    raise ImportError("windows_serial is only available on Windows")


GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x00000080
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
PURGE_TXCLEAR = 0x0004
PURGE_RXCLEAR = 0x0008
MAXDWORD = 0xFFFFFFFF


class DCB(ctypes.Structure):
    _fields_ = [
        ("DCBlength", wintypes.DWORD),
        ("BaudRate", wintypes.DWORD),
        ("flags", wintypes.DWORD),
        ("wReserved", wintypes.WORD),
        ("XonLim", wintypes.WORD),
        ("XoffLim", wintypes.WORD),
        ("ByteSize", wintypes.BYTE),
        ("Parity", wintypes.BYTE),
        ("StopBits", wintypes.BYTE),
        ("XonChar", ctypes.c_char),
        ("XoffChar", ctypes.c_char),
        ("ErrorChar", ctypes.c_char),
        ("EofChar", ctypes.c_char),
        ("EvtChar", ctypes.c_char),
        ("wReserved1", wintypes.WORD),
    ]


class COMMTIMEOUTS(ctypes.Structure):
    _fields_ = [
        ("ReadIntervalTimeout", wintypes.DWORD),
        ("ReadTotalTimeoutMultiplier", wintypes.DWORD),
        ("ReadTotalTimeoutConstant", wintypes.DWORD),
        ("WriteTotalTimeoutMultiplier", wintypes.DWORD),
        ("WriteTotalTimeoutConstant", wintypes.DWORD),
    ]


_kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
_kernel32.CreateFileW.argtypes = [
    wintypes.LPCWSTR,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.LPVOID,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.HANDLE,
]
_kernel32.CreateFileW.restype = wintypes.HANDLE
_kernel32.BuildCommDCBAndTimeoutsW.argtypes = [
    wintypes.LPCWSTR,
    ctypes.POINTER(DCB),
    ctypes.POINTER(COMMTIMEOUTS),
]
_kernel32.BuildCommDCBAndTimeoutsW.restype = wintypes.BOOL
_kernel32.SetCommState.argtypes = [wintypes.HANDLE, ctypes.POINTER(DCB)]
_kernel32.SetCommState.restype = wintypes.BOOL
_kernel32.SetCommTimeouts.argtypes = [wintypes.HANDLE, ctypes.POINTER(COMMTIMEOUTS)]
_kernel32.SetCommTimeouts.restype = wintypes.BOOL
_kernel32.SetupComm.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD]
_kernel32.SetupComm.restype = wintypes.BOOL
_kernel32.PurgeComm.argtypes = [wintypes.HANDLE, wintypes.DWORD]
_kernel32.PurgeComm.restype = wintypes.BOOL
_kernel32.ReadFile.argtypes = [
    wintypes.HANDLE,
    wintypes.LPVOID,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    wintypes.LPVOID,
]
_kernel32.ReadFile.restype = wintypes.BOOL
_kernel32.WriteFile.argtypes = [
    wintypes.HANDLE,
    wintypes.LPCVOID,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    wintypes.LPVOID,
]
_kernel32.WriteFile.restype = wintypes.BOOL
_kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
_kernel32.CloseHandle.restype = wintypes.BOOL
_kernel32.QueryDosDeviceW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD]
_kernel32.QueryDosDeviceW.restype = wintypes.DWORD


def _raise_last_error(action: str) -> None:
    error = ctypes.WinError(ctypes.get_last_error())
    raise OSError(f"{action}: {error}") from error


def _on_off(value: bool) -> str:
    return "on" if value else "off"


class WindowsSerial:
    """Synchronous Win32 serial reader with the subset used by serial_logger."""

    def __init__(
        self,
        *,
        port: str,
        baudrate: int,
        bytesize: int,
        parity: str,
        stopbits: float,
        timeout: float,
        xonxoff: bool,
        rtscts: bool,
        dsrdtr: bool,
    ) -> None:
        self.port = port
        self._handle: Optional[int] = None
        device_path = port if port.startswith("\\\\.\\") else f"\\\\.\\{port}"
        handle = _kernel32.CreateFileW(
            device_path,
            GENERIC_READ | GENERIC_WRITE,
            0,
            None,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            None,
        )
        if handle == INVALID_HANDLE_VALUE:
            _raise_last_error(f"无法打开 {port}")
        self._handle = handle

        dcb = DCB()
        dcb.DCBlength = ctypes.sizeof(DCB)
        timeouts = COMMTIMEOUTS()
        settings = (
            f"baud={baudrate} parity={parity.lower()} data={bytesize} stop={stopbits} "
            f"xon={_on_off(xonxoff)} octs={_on_off(rtscts)} "
            f"odsr={_on_off(dsrdtr)} dtr={'hs' if dsrdtr else 'off'} "
            f"rts={'hs' if rtscts else 'off'}"
        )
        try:
            if not _kernel32.BuildCommDCBAndTimeoutsW(settings, ctypes.byref(dcb), ctypes.byref(timeouts)):
                _raise_last_error("构造串口配置失败")
            if not _kernel32.SetCommState(handle, ctypes.byref(dcb)):
                _raise_last_error("设置串口参数失败")

            timeout_ms = max(1, int(float(timeout) * 1000))
            timeouts.ReadIntervalTimeout = MAXDWORD
            timeouts.ReadTotalTimeoutMultiplier = 0
            timeouts.ReadTotalTimeoutConstant = timeout_ms
            timeouts.WriteTotalTimeoutMultiplier = 0
            timeouts.WriteTotalTimeoutConstant = timeout_ms
            if not _kernel32.SetCommTimeouts(handle, ctypes.byref(timeouts)):
                _raise_last_error("设置串口超时失败")
            if not _kernel32.SetupComm(handle, 65536, 4096):
                _raise_last_error("设置串口缓冲区失败")
            if not _kernel32.PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR):
                _raise_last_error("清空串口缓冲区失败")
        except Exception:
            self.close()
            raise

    def read(self, size: int) -> bytes:
        if self._handle is None:
            raise OSError("串口已经关闭")
        buffer = ctypes.create_string_buffer(size)
        received = wintypes.DWORD(0)
        if not _kernel32.ReadFile(
            self._handle,
            buffer,
            size,
            ctypes.byref(received),
            None,
        ):
            _raise_last_error(f"读取 {self.port} 失败")
        return buffer.raw[: received.value]

    def write(self, data: bytes) -> int:
        if self._handle is None:
            raise OSError("串口已经关闭")
        payload = bytes(data)
        if not payload:
            return 0
        buffer = ctypes.create_string_buffer(payload)
        written = wintypes.DWORD(0)
        if not _kernel32.WriteFile(
            self._handle,
            buffer,
            len(payload),
            ctypes.byref(written),
            None,
        ):
            _raise_last_error(f"写入 {self.port} 失败")
        return int(written.value)

    def close(self) -> None:
        if self._handle is not None:
            _kernel32.CloseHandle(self._handle)
            self._handle = None


@dataclass(frozen=True)
class PortInfo:
    device: str
    description: str
    hwid: str
    vid: None = None
    pid: None = None


def comports() -> list[PortInfo]:
    """List DOS COM device names without opening the ports."""
    buffer_size = 65536
    buffer = ctypes.create_unicode_buffer(buffer_size)
    length = _kernel32.QueryDosDeviceW(None, buffer, buffer_size)
    if length == 0:
        _raise_last_error("枚举串口失败")
    names = buffer[:length].split("\x00")
    ports = sorted(
        (name for name in names if re.fullmatch(r"COM\d+", name, re.IGNORECASE)),
        key=lambda name: int(name[3:]),
    )
    return [PortInfo(port.upper(), "Windows COM port", "QueryDosDevice") for port in ports]
