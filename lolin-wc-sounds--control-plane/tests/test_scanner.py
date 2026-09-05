from wc_control.scanner import looks_like_wc_sounds


def test_status_json_from_firmware_is_a_module():
    status = {
        "time": "12:00:00",
        "time_ok": True,
        "playing": False,
        "file": "",
        "directory": "/birds_sounds",
        "volume": 70,
        "sd_ok": True,
        "wifi_sta": True,
        "ip": "192.168.1.42",
        "motion": False,
    }
    assert looks_like_wc_sounds(status) is True


def test_random_http_json_is_not_a_module():
    assert looks_like_wc_sounds({"ok": True, "ip": "1.2.3.4"}) is False
    assert looks_like_wc_sounds({}) is False
    assert looks_like_wc_sounds(None) is False
