from __future__ import annotations

import io
import subprocess
import tempfile
import wave
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True)
class WavTarget:
    sample_rate: int = 16000
    channels: int = 1
    bits: int = 16
    audio_format: int = 1


TARGET = WavTarget()


@dataclass
class WavInfo:
    ok: bool
    needs_conversion: bool
    sample_rate: int | None
    channels: int | None
    bits: int | None
    audio_format: int | None
    reason: str

    def as_dict(self) -> dict:
        data = asdict(self)
        data["target"] = asdict(TARGET)
        return data


def needs_conversion(info: WavInfo) -> bool:
    return info.needs_conversion


def _matches_target(
    sample_rate: int, channels: int, bits: int, audio_format: int
) -> bool:
    return (
        sample_rate == TARGET.sample_rate
        and channels == TARGET.channels
        and bits == TARGET.bits
        and audio_format == TARGET.audio_format
    )


def _riff_fmt(data: bytes) -> tuple[int, int, int, int] | None:
    if len(data) < 44:
        return None
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        return None
    fmt = None
    i = 12
    while i + 8 <= len(data):
        chunk_id = data[i : i + 4]
        chunk_size = int.from_bytes(data[i + 4 : i + 8], "little")
        start = i + 8
        if chunk_id == b"fmt " and start + 16 <= len(data):
            audio_format = int.from_bytes(data[start : start + 2], "little")
            channels = int.from_bytes(data[start + 2 : start + 4], "little")
            rate = int.from_bytes(data[start + 4 : start + 8], "little")
            bits = int.from_bytes(data[start + 14 : start + 16], "little")
            fmt = (audio_format, channels, rate, bits)
            break
        i = start + chunk_size
        if chunk_size % 2:
            i += 1
    return fmt


def analyze_wav(data: bytes) -> WavInfo:
    fmt = _riff_fmt(data)
    if fmt is None:
        try:
            with wave.open(io.BytesIO(data), "rb") as wav:
                channels = wav.getnchannels()
                bits = wav.getsampwidth() * 8
                rate = wav.getframerate()
            fmt = (1, channels, rate, bits)
        except Exception:
            return WavInfo(
                ok=False,
                needs_conversion=True,
                sample_rate=None,
                channels=None,
                bits=None,
                audio_format=None,
                reason="Это не WAV (нет RIFF/WAVE в заголовке)",
            )
    audio_format, channels, rate, bits = fmt
    if _matches_target(rate, channels, bits, audio_format):
        return WavInfo(
            ok=True,
            needs_conversion=False,
            sample_rate=rate,
            channels=channels,
            bits=bits,
            audio_format=audio_format,
            reason="Уже WAV PCM 16-bit 16 kHz mono — модуль примет файл как есть",
        )
    return WavInfo(
        ok=True,
        needs_conversion=True,
        sample_rate=rate,
        channels=channels,
        bits=bits,
        audio_format=audio_format,
        reason=(
            "Модулю нужен WAV 16-bit, 16 kHz, mono. "
            f"Сейчас: {bits}-bit, {rate} Hz, {channels} канал(ов)"
        ),
    )


def convert_to_module_wav(data: bytes, ffmpeg: str = "ffmpeg") -> bytes:
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / "in.bin"
        dst = Path(tmp) / "out.wav"
        src.write_bytes(data)
        cmd = [
            ffmpeg,
            "-y",
            "-i",
            str(src),
            "-ar",
            str(TARGET.sample_rate),
            "-ac",
            str(TARGET.channels),
            "-c:a",
            "pcm_s16le",
            str(dst),
        ]
        result = subprocess.run(
            cmd, capture_output=True, text=True, check=False
        )
        if result.returncode != 0 or not dst.is_file():
            err = (result.stderr or result.stdout or "ffmpeg не смог конвертировать").strip()
            raise RuntimeError(err[-800:])
        converted = dst.read_bytes()
    info = analyze_wav(converted)
    if not info.ok or info.needs_conversion:
        raise RuntimeError("после ffmpeg файл всё ещё не 16 kHz 16-bit mono WAV")
    return converted
