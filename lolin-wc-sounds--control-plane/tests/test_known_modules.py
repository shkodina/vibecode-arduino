import asyncio
from pathlib import Path

import httpx

from wc_control.scanner import check_known_modules
from wc_control.settings import load_settings, save_discovered_modules

STATUS = {
    "time": "12:00:00",
    "time_ok": True,
    "playing": False,
    "file": "",
    "directory": "/data",
    "volume": 10,
    "sd_ok": True,
    "wifi_sta": True,
    "ip": "192.168.10.5",
    "motion": False,
}


def test_save_discovered_modules_keeps_comments_and_subnet(tmp_path: Path):
    cfg = tmp_path / "config.yaml"
    cfg.write_text(
        """# keep this comment
listen:
  host: 127.0.0.1
  port: 8000
scan:
  subnet: 192.168.10.0/24
  port: 80
""",
        encoding="utf-8",
    )
    save_discovered_modules(
        cfg,
        [{"host": "192.168.10.5", "port": 80}, {"host": "192.168.10.8", "port": 80}],
    )
    text = cfg.read_text(encoding="utf-8")
    assert "# keep this comment" in text
    assert "192.168.10.0/24" in text
    settings = load_settings(cfg)
    assert settings.known_modules == (("192.168.10.5", 80), ("192.168.10.8", 80))
    assert settings.host_allowed("192.168.10.5") is True
    save_discovered_modules(cfg, [{"host": "192.168.10.5", "port": 80}])
    again = load_settings(cfg)
    assert again.known_modules == (("192.168.10.5", 80),)
    assert cfg.read_text(encoding="utf-8").count("192.168.10.8") == 0


def test_check_known_marks_offline_and_online():
    def handler(request: httpx.Request) -> httpx.Response:
        if "192.168.10.5" in str(request.url):
            return httpx.Response(200, json=STATUS)
        return httpx.Response(404)

    async def go():
        transport = httpx.MockTransport(handler)
        async with httpx.AsyncClient(transport=transport) as client:
            return await check_known_modules(
                [("192.168.10.5", 80), ("192.168.10.9", 80)],
                timeout_seconds=0.2,
                client=client,
            )

    rows = asyncio.run(go())
    assert rows[0]["host"] == "192.168.10.5"
    assert rows[0]["online"] is True
    assert rows[1]["host"] == "192.168.10.9"
    assert rows[1]["online"] is False
    assert rows[1]["status"] is None
