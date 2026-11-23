#!/usr/bin/env python3
"""Package a PlatformIO ESP32 build for ESP Web Tools. Does not flash hardware."""
import json
import os
import shutil
from pathlib import Path

root = Path(__file__).resolve().parents[2]
build = root / ".pio" / "build" / "ESP32"
output = root / "dist" / "webinstaller-esp32"
output.mkdir(parents=True, exist_ok=True)

parts = {
    "bootloader.bin": (build / "bootloader.bin", 0x1000),
    "partitions.bin": (build / "partitions.bin", 0x8000),
    "firmware.bin": (build / "firmware.bin", 0x10000),
}
for filename, (source, _) in parts.items():
    if not source.is_file() or source.stat().st_size == 0:
        raise SystemExit(f"Missing or empty firmware image: {source}")
    shutil.copy2(source, output / filename)

boot_app_candidates = [
    Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin",
]
boot_app = next((p for p in boot_app_candidates if p.is_file()), None)
if boot_app is None:
    raise SystemExit("Missing boot_app0.bin in PlatformIO ESP32 framework")
shutil.copy2(boot_app, output / "boot_app0.bin")

parts["boot_app0.bin"] = (boot_app, 0xE000)
manifest = {
    "name": "Multilayout ESP Wordclock",
    "version": os.environ.get("GITHUB_SHA", "development")[:12],
    "new_install_prompt_erase": False,
    "builds": [{
        "chipFamily": "ESP32",
        "parts": [
            {"path": name, "offset": offset}
            for name, (_, offset) in sorted(parts.items(), key=lambda x: x[1][1])
        ],
    }],
}
(output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
shutil.copy2(root / "docs/install/index.html", output / "index.html")
print(f"ESP32 installer bundle prepared at {output}")
