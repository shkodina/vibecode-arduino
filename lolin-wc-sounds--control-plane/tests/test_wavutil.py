import io
import wave

from wc_control.wavutil import TARGET, analyze_wav, needs_conversion


def _pcm_wav_bytes(rate: int, channels: int, sampwidth: int, frames: int = 160) -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(sampwidth)
        w.setframerate(rate)
        silence = b"\x00" * (frames * channels * sampwidth)
        w.writeframes(silence)
    return buf.getvalue()


def test_module_format_does_not_need_conversion():
    data = _pcm_wav_bytes(16000, 1, 2)
    info = analyze_wav(data)
    assert info.ok is True
    assert info.needs_conversion is False
    assert info.sample_rate == 16000
    assert info.channels == 1
    assert info.bits == 16
    assert needs_conversion(info) is False


def test_stereo_44k_needs_conversion():
    data = _pcm_wav_bytes(44100, 2, 2)
    info = analyze_wav(data)
    assert info.ok is True
    assert info.needs_conversion is True
    assert info.sample_rate == 44100
    assert info.channels == 2
    assert needs_conversion(info) is True


def test_mp3_header_is_not_module_wav():
    data = b"ID3\x04\x00\x00" + b"\x00" * 64
    info = analyze_wav(data)
    assert info.ok is False
    assert info.needs_conversion is True
    assert "WAV" in info.reason or "RIFF" in info.reason


def test_target_matches_firmware():
    assert TARGET.sample_rate == 16000
    assert TARGET.channels == 1
    assert TARGET.bits == 16
    assert TARGET.audio_format == 1
