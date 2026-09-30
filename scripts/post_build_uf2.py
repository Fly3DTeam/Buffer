Import("env")

from pathlib import Path
import subprocess
import sys


BIN2UF2_SCRIPT = Path(r"scripts/bin2uf2.py")
FLASHTOOL_SCRIPT = Path(r"scripts/flashtool.py")
BOOTLOADER_DIR = Path(r"bl")
APPLICATION_BASE_ADDRESS = "0x08002000"
APPLICATION_OFFSET = 0x2000


def create_merged_bin(firmware_bin: Path, env) -> Path:
    project_dir = Path(env.subst("$PROJECT_DIR")).resolve()
    flashtool_script = project_dir / FLASHTOOL_SCRIPT
    bootloader_dir = project_dir / BOOTLOADER_DIR
    bootloaders = sorted(bootloader_dir.glob("*.bin"))
    if len(bootloaders) != 1:
        raise RuntimeError(
            f"expected exactly one bootloader binary in {bootloader_dir}, "
            f"found {len(bootloaders)}"
        )
    if not flashtool_script.is_file():
        raise RuntimeError(f"flashtool script not found: {flashtool_script}")

    merged_bin = firmware_bin.with_name(
        f"{firmware_bin.stem}(mergedBL){firmware_bin.suffix}"
    )
    subprocess.run(
        [
            sys.executable,
            str(flashtool_script),
            "--merge",
            "--firmware",
            str(firmware_bin),
            "--bootloader",
            str(bootloaders[0]),
            "--output",
            str(merged_bin),
            "--app-offset",
            hex(APPLICATION_OFFSET),
        ],
        check=True,
    )
    print(f"Merged firmware generated: {merged_bin}")
    return merged_bin


def create_uf2(source, target, env):
    # The post action is attached to firmware.bin. SCons passes that file as
    # the action target and the ELF file as its source.
    firmware_bin = Path(str(target[0])).resolve()
    firmware_uf2 = firmware_bin.with_suffix(".uf2")

    create_merged_bin(firmware_bin, env)

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
