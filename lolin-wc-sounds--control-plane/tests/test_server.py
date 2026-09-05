from fastapi.testclient import TestClient

from wc_control.server import create_app
from wc_control.settings import Settings


def _settings() -> Settings:
    return Settings(
        listen_host="127.0.0.1",
        listen_port=8000,
        scan_subnet="192.168.1.0/24",
        scan_port=80,
        timeout_seconds=0.2,
        concurrency=8,
        hosts=("192.168.1.42", "192.168.1.43"),
    )


def test_root_serves_ui():
    client = TestClient(create_app(_settings()))
    response = client.get("/")
    assert response.status_code == 200
    assert "WC Sounds" in response.text


def test_known_empty_list():
    client = TestClient(create_app(_settings()))
    response = client.get("/api/known")
    assert response.status_code == 200
    assert response.json()["modules"] == []


def test_analyze_flags_wrong_wav():
    import io
    import wave

    buf = io.BytesIO()
    with wave.open(buf, "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(44100)
        wav.writeframes(b"\x00" * 256)
    client = TestClient(create_app(_settings()))
    response = client.post(
        "/api/analyze",
        files={"file": ("stereo.wav", buf.getvalue(), "audio/wav")},
    )
    assert response.status_code == 200
    body = response.json()
    assert body["needs_conversion"] is True
    assert body["sample_rate"] == 44100
    assert body["module_filename"] == "stereo.wav"


def test_convert_cyrillic_filename_does_not_crash():
    import io
    import wave

    buf = io.BytesIO()
    with wave.open(buf, "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(44100)
        wav.writeframes(b"\x00" * 512)
    client = TestClient(create_app(_settings()))
    response = client.post(
        "/api/convert",
        files={"file": ("журчание ручья в лесу.mp3", buf.getvalue(), "audio/mpeg")},
    )
    assert response.status_code == 200
    assert "zhurchanie-ruchya-v-lesu.wav" in response.headers.get("content-disposition", "")
    assert len(response.content) > 44

