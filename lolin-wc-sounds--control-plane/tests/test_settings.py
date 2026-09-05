from pathlib import Path

import pytest

from wc_control.settings import SettingsError, load_settings


def test_load_settings_reads_subnet_and_listen(tmp_path: Path):
    cfg = tmp_path / "config.yaml"
    cfg.write_text(
        """
listen:
  host: 127.0.0.1
  port: 8000
scan:
  subnet: 192.168.10.0/24
  port: 80
  timeout_seconds: 0.4
  concurrency: 32
""",
        encoding="utf-8",
    )

    settings = load_settings(cfg)

    assert settings.listen_host == "127.0.0.1"
    assert settings.listen_port == 8000
    assert settings.scan_subnet == "192.168.10.0/24"
    assert settings.scan_port == 80
    assert settings.timeout_seconds == 0.4
    assert settings.concurrency == 32
    assert len(settings.hosts) == 254
    assert settings.hosts[0] == "192.168.10.1"
    assert settings.hosts[-1] == "192.168.10.254"


def test_load_settings_rejects_missing_file(tmp_path: Path):
    with pytest.raises(SettingsError):
        load_settings(tmp_path / "nope.yaml")


def test_host_allowed_only_inside_subnet(tmp_path: Path):
    cfg = tmp_path / "config.yaml"
    cfg.write_text(
        """
listen:
  host: 127.0.0.1
  port: 8000
scan:
  subnet: 10.8.0.0/29
  port: 80
""",
        encoding="utf-8",
    )
    settings = load_settings(cfg)

    assert settings.host_allowed("10.8.0.1") is True
    assert settings.host_allowed("10.8.0.99") is False
    assert settings.host_allowed("not-an-ip") is False
