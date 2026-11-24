"""Build installer packages with validated default flash layouts."""
import json
import hashlib
import shutil
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[2]
out = root / "dist/webinstaller-esp32"
out.mkdir(parents=True, exist_ok=True)
version = json.loads((root / "package.json").read_text())["version"]
builds = []
for env, family, chip in [("ESP32", "ESP32", 0), ("ESP32C3", "ESP32-C3", 5), ("ESP8266", "ESP8266", None)]:
    directory = root / "firmware-staging" / env
    firmware = directory / "firmware.bin"
    data = firmware.read_bytes()
    if len(data) < 24 or data[0] != 0xe9:
        raise ValueError(f"{env}: invalid firmware header")
    if chip is not None and int.from_bytes(data[12:14], "little") != chip:
        raise ValueError(f"{env}: unexpected chip ID")
    images = [(0, firmware)]
    if chip is not None:
        table = directory / "partitions.bin"
        entries = []
        raw = table.read_bytes()
        for pos in range(0, len(raw)-31, 32):
            magic, typ, sub, offset, size, label, flags = struct.unpack_from("<HBBII16sI", raw, pos)
            if magic != 0x50aa:
                break
            entries.append((typ, sub, offset, size))
        apps = [e for e in entries if e[0] == 0 and e[1] in (0, 16)]
        ota = [e for e in entries if e[:2] == (1, 0)]
        if len(apps) != 1 or len(ota) != 1:
            raise ValueError(f"{env}: unsupported partition layout")
        if len(data) > apps[0][3]:
            raise ValueError(f"{env}: firmware exceeds app partition")
        boot = Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
        if boot.stat().st_size > ota[0][3]:
            raise ValueError("OTA initializer too large")
        images = [(0x1000 if chip == 0 else 0, directory / "bootloader.bin"), (0x8000, table), (ota[0][2], boot), (apps[0][2], firmware)]
    parts = []
    end = 0
    for offset, source in sorted(images):
        if not source.is_file() or not source.stat().st_size or offset < end:
            raise ValueError(f"{env}: missing or overlapping image {source}")
        end = offset + source.stat().st_size
        target = out / env / source.name
        target.parent.mkdir(exist_ok=True)
        shutil.copy2(source, target)
        parts.append({"path": f"{env}/{source.name}", "offset": offset})
    builds.append({"chipFamily": family, "parts": parts})
    print(f"{env}: firmware {len(data)} bytes")
(out / "manifest.json").write_text(json.dumps({"name":"Multilayout ESP Wordclock","version":version,"new_install_prompt_erase":False,"builds":builds}, indent=2))
shutil.copy2(root / "docs/install/index.html", out / "index.html")
(out / "SHA256SUMS.txt").write_text("\n".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(out)}" for p in sorted(out.rglob("*")) if p.is_file() and p.name != "SHA256SUMS.txt")+"\n")
