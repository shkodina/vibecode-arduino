from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(name: str) -> str:
    return (ROOT / name).read_text(encoding="utf-8")


def test_firmware_contains_required_services_and_storage():
    firmware = read("silomer-stanovaya.ino")

    required_tokens = [
        "#include <ESP8266WiFi.h>",
        "#include <ESP8266WebServer.h>",
        "#include <EEPROM.h>",
        "#include <HX711.h>",
        "#include <ArduinoJson.h>",
        "#include <WebSocketsServer.h>",
        "server.on(\"/api/config\"",
        "server.on(\"/api/status\"",
        "server.on(\"/api/reset\"",
        "server.on(\"/api/reboot\"",
        "server.on(\"/api/tare\"",
        "server.on(\"/api/calibrate\"",
        "server.on(\"/swagger\"",
        "webSocket.begin()",
        "EEPROM.commit()",
        "PIN_REZHIM",
        "WIFI_SSID",
        "WIFI_PASS",
    ]

    for token in required_tokens:
        assert token in firmware


def test_firmware_formats_weight_and_units_for_page():
    firmware = read("silomer-stanovaya.ino")

    assert "tekushiyVesKg" in firmware
    assert "maxTekushiyKg" in firmware
    assert "maxProshliyKg" in firmware
    assert "кг" in firmware
    assert "сек" in firmware
    assert ".toFixed(1)" in firmware


def test_firmware_shows_visible_feedback_after_web_actions():
    firmware = read("silomer-stanovaya.ino")

    required_tokens = [
        'id="actionStatus"',
        "function showActionStatus(",
        "classList.add('pressed')",
        "Настройки сохранены",
        "Tare выполнен",
        "Калибровка выполнена",
        "Сброс выполнен",
        "Watchdog-перезагрузка запрошена",
    ]

    for token in required_tokens:
        assert token in firmware


def test_firmware_exposes_named_wifi_modes_and_calibration_config():
    firmware = read("silomer-stanovaya.ino")
    config = read("config.h")

    required_firmware_tokens = [
        "UseExistedWiFi",
        "StandAlone",
        "calibrationScale",
        "calibrationOffset",
        "sensorMaxKg",
        "vesy.set_scale(config.calibrationScale)",
        "vesy.set_offset(config.calibrationOffset)",
        "doc[\"calibrationScale\"]",
        "doc[\"calibrationOffset\"]",
        "doc[\"sensorMaxKg\"]",
    ]
    required_config_tokens = [
        "ZAVOD_HX711_SCALE = 4718.0f",
        "ZAVOD_HX711_OFFSET",
        "ZAVOD_SENSOR_MAX_KG",
    ]

    for token in required_firmware_tokens:
        assert token in firmware
    for token in required_config_tokens:
        assert token in config


def test_firmware_exposes_read_only_model_id_for_admin_scan():
    firmware = read("silomer-stanovaya.ino")
    config = read("config.h")
    readme = read("readme.md").lower()

    assert 'MODEL_ID = "silomer-stanovaya"' in config
    assert 'doc["modelId"] = MODEL_ID' in firmware
    assert 'id="modelId"' not in firmware
    cfg_submit = firmware.split("qs('cfg').addEventListener")[1].split("qs('cal').addEventListener")[0]
    assert "modelId:" not in cfg_submit
    assert "modelid" in readme
    assert "не путать с именем устройства" in readme or "не путать с `devicename`" in readme


def test_firmware_waits_for_hx711_before_tare_and_calibration():
    firmware = read("silomer-stanovaya.ino")

    required_tokens = [
        "HX711_READY_TIMEOUT_MS",
        "zhdatGotovnostHX711(",
        "while (!vesy.is_ready()",
        "yield();",
        "if (!zhdatGotovnostHX711())",
    ]

    for token in required_tokens:
        assert token in firmware


def test_build_script_uses_env_and_does_not_store_wifi_in_sources():
    script = read("sborka.sh")
    env_example = read(".env.primer")
    gitignore = (ROOT.parent / ".gitignore").read_text(encoding="utf-8")

    assert "source .env" in script
    assert "-DWIFI_SSID" in script
    assert "-DWIFI_PASS" in script
    assert "config.h" in script
    assert "silomer-stanovaya.ino" in script
    assert "WIFI_SSID=" in env_example
    assert "WIFI_PASS=" in env_example
    assert ".env" in gitignore


def test_readme_documents_wiring_api_and_next_steps():
    readme = read("readme.md").lower()

    required_phrases = [
        "схема подключения",
        "lolin nodemcu v3",
        "yzc-516c",
        "hx711",
        "d2",
        "d1",
        "d5",
        "standalone",
        "useexistedwifi",
        "d5` на `gnd",
        "d5` на `3v3",
        "/api/config",
        "/api/status",
        "/api/tare",
        "/api/calibrate",
        "/swagger",
        "/api/ws",
        "нижний статус",
        "1200 мс",
        "hx711ready",
        "4478.333",
        "калибров",
        "доработ",
    ]

    for phrase in required_phrases:
        assert phrase in readme
