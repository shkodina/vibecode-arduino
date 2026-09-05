import io
import wave

from wc_control.wavutil import analyze_wav, convert_to_module_wav


def _pcm_wav_bytes(rate: int, channels: int, sampwidth: int, frames: int = 320) -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(sampwidth)
        wav.setframerate(rate)
        wav.writeframes(b"\x00" * (frames * channels * sampwidth))
    return buf.getvalue()


def test_ffmpeg_converts_44k_stereo_to_module_format():
    source = _pcm_wav_bytes(44100, 2, 2)
    converted = convert_to_module_wav(source)
    info = analyze_wav(converted)
    assert info.ok is True
    assert info.needs_conversion is False
    assert info.sample_rate == 16000
    assert info.channels == 1
    assert info.bits == 16
