#!/usr/bin/env python3
"""Package the development UI and the three PlatformIO builds for ESP Web Tools."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).resolve().parent
TARGETS = {
    "ESP8266": ("ESP8266", "esp8266", None, 0),
    "ESP32": ("ESP32", "esp32", 0, 0x1000),
    "ESP32C3": ("ESP32-C3", "esp32c3", 5, 0),
}


def validate_image(data, chip_id, name):
    if len(data) < 16 or data[0] != 0xE9 or not 0 < data[1] <= 16:
        raise ValueError(f"Unexpected ESP image header: {name}")
    if chip_id is not None and int.from_bytes(data[12:14], "little") != chip_id:
        raise ValueError(f"Image belongs to a different ESP family: {name}")


def validate_partitions(data, app_size):
    entries = []
    for offset in range(0, len(data) - 31, 32):
        if data[offset:offset + 2] != b"\xaa\x50":
            break
        entries.append(struct.unpack_from("<HBBII16sI", data, offset))
    if not entries or not any(
        kind == 0 and flash_offset == 0x10000 and size >= app_size
        for _, kind, _, flash_offset, size, _, _ in entries
    ):
        raise ValueError("Partition table has no compatible application slot at 0x10000")
    if not any(
        kind == 1 and subtype == 0 and flash_offset == 0xE000 and size >= 8192
        for _, kind, subtype, flash_offset, size, _, _ in entries
    ):
        raise ValueError("Partition table has no OTA data slot at 0xE000")


def package_board(board, output, build, core, version):
    family, directory, chip_id, boot_offset = TARGETS[board]
    folder = output / directory
    folder.mkdir()
    source = build / board
    firmware = (source / "firmware.bin").read_bytes()
    validate_image(firmware, chip_id, f"{board}/firmware.bin")
    if version.encode("ascii") + b"\0" not in firmware:
        raise ValueError(f"{board} was not built with version {version}")
    if chip_id is None:
        if len(firmware) > 0x100000:
            raise ValueError(f"{board} image exceeds one MiB")
        images = [(0, "firmware.bin", firmware)]
    else:
        bootloader = (source / "bootloader.bin").read_bytes()
        partitions = (source / "partitions.bin").read_bytes()
        boot_app = (core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin").read_bytes()
        validate_image(bootloader, chip_id, f"{board}/bootloader.bin")
        validate_partitions(partitions, len(firmware))
        if len(boot_app) != 8192:
            raise ValueError("Unexpected Arduino boot_app0 size")
        images = [
            (boot_offset, "bootloader.bin", bootloader),
            (0x8000, "partitions.bin", partitions),
            (0xE000, "boot_app0.bin", boot_app),
            (0x10000, "firmware.bin", firmware),
        ]

    for index, (offset, name, data) in enumerate(images):
        if index + 1 < len(images) and offset + len(data) > images[index + 1][0]:
            raise ValueError(f"{board}/{name} overlaps the next flash region")
        (folder / name).write_bytes(data)

    manifest = {
        "name": "Wordclock Development",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [{
            "chipFamily": family,
            "parts": [{"path": name, "offset": offset} for offset, name, _ in images],
        }],
    }
    (folder / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    checksums = "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n" for _, name, data in images)
    (folder / "SHA256SUMS.txt").write_text(checksums, encoding="ascii")


def build_demo(output, page):
    output.mkdir()
    content = page.read_text(encoding="utf-8")
    panel = (SOURCE / "demo-panel.html").read_text(encoding="utf-8")
    content = content.replace("</head>", '<link rel="stylesheet" href="./demo.css"><script src="./demo.js"></script></head>', 1)
    content, replaced = re.subn(r'(<main class="?main"?>)', r"\1" + panel, content, count=1)
    if replaced != 1 or './demo.js' not in content:
        raise ValueError("Could not attach the demo to the generated firmware UI")
    content = re.sub(r'(<span id="?pioenv"? class="?env"?>)[^<]*', r'\1Demo', content, count=1)
    (output / "index.html").write_text(content, encoding="utf-8")
    for name in ("demo.js", "demo.css"):
        shutil.copy2(SOURCE / name, output / name)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=ROOT / "dist/development")
    parser.add_argument("--demo-only", action="store_true")
    args = parser.parse_args()
    build = ROOT / ".pio/build"
    core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    version = json.loads((ROOT / "package.json").read_text(encoding="utf-8"))["version"]
    sha = subprocess.check_output(["git", "rev-parse", "--short=12", "HEAD"], cwd=ROOT, text=True).strip()
    if "-dev." not in version:
        raise SystemExit("Development packages require a development firmware version")
    args.output.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(dir=args.output.parent) as tmp:
        stage = Path(tmp) / "site"
        stage.mkdir()
        build_demo(stage / "demo", build / "webpage/index.html")
        if not args.demo_only:
            (stage / "firmware").mkdir()
            for board in TARGETS:
                package_board(board, stage / "firmware", build, core, version)
            shutil.copy2(SOURCE / "installer.html", stage / "index.html")
            (stage / "build.json").write_text(json.dumps({"version": version, "commit": sha}) + "\n", encoding="utf-8")
            with zipfile.ZipFile(stage / "wordclock-development.zip", "w", zipfile.ZIP_DEFLATED) as bundle:
                for file in sorted(stage.rglob("*")):
                    if file.is_file() and file.suffix != ".zip":
                        bundle.write(file, file.relative_to(stage))
        if args.output.exists():
            # Replace only generated files, preserving a surrounding deployment project.
            for entry in stage.iterdir():
                dest = args.output / entry.name
                if dest.is_dir():
                    shutil.rmtree(dest)
                entry.replace(dest)
        else:
            stage.replace(args.output)
    print(args.output)


if __name__ == "__main__":
    main()
