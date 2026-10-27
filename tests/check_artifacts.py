"""Validate downloaded Actions outputs, never build firmware locally."""
from pathlib import Path
import hashlib
import json
import sys

root = Path(sys.argv[1])
sha = sys.argv[2]
names = ["voltball_left", "voltball_right", "prospector", "voltball_left_settings_reset",
         "voltball_right_settings_reset", "prospector_settings_reset"]
report = {}
for name in names:
    def text(suffix):
        return (root / (name + suffix)).read_text(encoding="utf-8")
    assert text("-source-sha.txt").strip() == sha, name
    conf = dict(line.split("=", 1) for line in text(".config").splitlines()
                if line.startswith("CONFIG_") and "=" in line)
    def on(key): return conf.get("CONFIG_" + key) == "y"
    data = (root / (name + ".uf2")).read_bytes()
    assert len(data) % 512 == 0 and data[:4] == bytes.fromhex("5546320a"), name
    assert "revision:" in text("-west.yml")
    assert "UINT8_MAX" in text("-prospector.patch")
    if name.endswith("settings_reset"):
        assert on("ZMK_SETTINGS_RESET_ON_START") and not on("ZMK_BLE"), name
    elif name == "prospector":
        assert on("ZMK_SPLIT_ROLE_CENTRAL") and on("ZMK_INPUT_SPLIT")
        assert conf["CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS"] == "2"
        assert on("ZMK_STUDIO") and on("ZMK_STUDIO_TRANSPORT_UART")
        assert not on("ZMK_STUDIO_TRANSPORT_BLE")
        assert on("ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING")
        for key in ["APDS9960", "PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR", "ZMK_NON_LIPO_BATTERY_MANAGEMENT",
                    "PAW3222", "ZMK_STATUS_LED", "ZMK_SLEEP"]:
            assert not on(key), (name, key)
        assert conf["CONFIG_PROSPECTOR_FIXED_BRIGHTNESS"] == "50"
        assert 'compatible = "zmk,non-lipo-battery"' not in text(".dts")
    else:
        assert not on("ZMK_SPLIT_ROLE_CENTRAL") and not on("ZMK_STUDIO")
        assert on("ZMK_NON_LIPO_BATTERY_MANAGEMENT") and on("ZMK_SLEEP")
        assert conf["CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT"] == "300000"
        assert on("PAW3222") == (name == "voltball_right")
        assert on("ZMK_INPUT_SPLIT") == (name == "voltball_right")
    report[name] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
assert len(list(root.glob("*.uf2"))) == 6
print(json.dumps({"source_sha": sha, "firmware": report, "result": "PASS"}, indent=2))
