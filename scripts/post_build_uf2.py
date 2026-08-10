Import("env")

from pathlib import Path
import subprocess
import sys


BIN2UF2_SCRIPT = Path(r"D:\mellowProject\固件集合\bootloader\bin2uf2.py")
APPLICATION_BASE_ADDRESS = "0x08002000"


def create_uf2(source, target, env):
    # The post action is attached to firmware.bin. SCons passes that file as
    # the action target and the ELF file as its source.
    firmware_bin = Path(str(target[0])).resolve()
    firmware_uf2 = firmware_bin.with_suffix(".uf2")

    if not BIN2UF2_SCRIPT.is_file():
        raise RuntimeError(f"bin2uf2 script not found: {BIN2UF2_SCRIPT}")

    subprocess.run(
        [
            sys.executable,
            str(BIN2UF2_SCRIPT),
            str(firmware_bin),
            str(firmware_uf2),
            "--base",
            APPLICATION_BASE_ADDRESS,
        ],
        check=True,
    )
    print(f"UF2 generated: {firmware_uf2}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", create_uf2)
