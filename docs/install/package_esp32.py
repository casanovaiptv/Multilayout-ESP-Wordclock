#!/usr/bin/env python3
"""Package a classic ESP32 PlatformIO build for ESP Web Tools.

This script never flashes a device and fails closed on missing or invalid images.
"""
import hashlib
import json
import os
import shutil
from pathlib import Path

root = Path(__file__).resolve().parents[2]
build = root / ".pio" / "build" / "ESP32"
output = root / "dist" / "webinstaller-esp32"
output.mkdir(parents=True, exist_ok=True)

images = [
    ("bootloader.bin", build / "bootloader.bin", 0x1000),
    ("partitions.bin", build / "partitions.bin", 0x8000),
    ("boot_app0.bin", Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin", 0xE000),
    ("firmware.bin", build / "firmware.bin", 0x10000),
]

# These offsets are for the default classic ESP32 Arduino partition layout.
# Reject unexpected partition sizes and images that overlap the next region.
for i, (name, source, offset) in enumerate(images):
    if not source.is_file() or source.stat().st_size == 0:
        raise SystemExit(f"Missing or empty image: {source}")
    if i < len(images) - 1:
        next_offset = images[i + 1][2]
        if offset + source.stat().st_size > next_offset:
            raise SystemExit(f"Image overlaps next flash region: {name}")
    shutil.copy2(source, output / name)

# Partition table must have the ESP-IDF binary partition-table magic.
partitions = (output / "partitions.bin").read_bytes()
if len(partitions) < 2 or partitions[:2] != b"\xaa\x50":
    raise SystemExit("Unexpected partition table format; check PlatformIO board configuration")

manifest = {
    "name": "Multilayout ESP Wordclock",
    "version": os.environ.get("GITHUB_SHA", "development")[:12],
    "new_install_prompt_erase": False,
    "builds": [{
        "chipFamily": "ESP32",
        "parts": [{"path": name, "offset": offset} for name, _, offset in images],
    }],
}
(output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
shutil.copy2(root / "docs/install/index.html", output / "index.html")

checksums = []
for file in sorted(output.iterdir()):
    if file.is_file():
        digest = hashlib.sha256(file.read_bytes()).hexdigest()
        checksums.append(f"{digest}  {file.name}")
(output / "SHA256SUMS.txt").write_text("\n".join(checksums) + "\n", encoding="utf-8")
print(f"ESP32 installer bundle prepared: {output}")
