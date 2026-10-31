# PlatformIO extra_script (runs inside PlatformIO's SCons Python, so it can't
# use uv/PEP 723 like the other tools/ scripts).
#
# Protects per-scarf settings stored in NVS (e.g. LED type) across firmware
# updates. USB uploads and OTA never write NVS, so settings survive — unless:
#   1. the partition table moves or resizes the nvs partition, or
#   2. the flash is fully erased.
# This script fails the build/upload in either case.

Import("env")  # noqa: F821 — provided by SCons

import csv
import os

EXPECTED_NVS_OFFSET = 0x9000
EXPECTED_NVS_SIZE   = 0x5000

if "erase" in COMMAND_LINE_TARGETS:  # noqa: F821
    print("\n*** pio_guard_nvs: refusing 'erase' — it wipes NVS (per-scarf LED type).")
    print("*** Remove extra_scripts guard in platformio.ini if you really mean it.\n")
    env.Exit(1)

csv_path = env.subst("$PARTITIONS_TABLE_CSV")
if not csv_path or not os.path.isfile(csv_path):
    print(f"\n*** pio_guard_nvs: can't find partition table ({csv_path!r}).\n")
    env.Exit(1)

nvs = None
with open(csv_path) as f:
    for row in csv.reader(line for line in f if not line.lstrip().startswith("#")):
        cols = [c.strip() for c in row]
        if len(cols) >= 5 and cols[0] == "nvs":
            nvs = (int(cols[3], 0), int(cols[4], 0))

if nvs != (EXPECTED_NVS_OFFSET, EXPECTED_NVS_SIZE):
    got = "missing" if nvs is None else f"offset=0x{nvs[0]:x} size=0x{nvs[1]:x}"
    print(f"\n*** pio_guard_nvs: nvs partition in {os.path.basename(csv_path)} is {got};")
    print(f"*** expected offset=0x{EXPECTED_NVS_OFFSET:x} size=0x{EXPECTED_NVS_SIZE:x}.")
    print("*** Changing it would lose every scarf's saved settings on update.\n")
    env.Exit(1)
