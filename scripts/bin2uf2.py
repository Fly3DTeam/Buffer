#!/usr/bin/env python3
"""Convert a flat application binary to the UF2 format accepted by FlyBoot."""

import argparse
import pathlib
import struct


UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
PAYLOAD_SIZE = 256


def integer(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert a flat firmware binary to UF2"
    )
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument(
        "-b", "--base", type=integer, required=True,
        help="application load address, for example 0x08004000",
    )
    parser.add_argument(
        "--family-id", type=integer,
        help="optional UF2 family ID (hex or decimal)",
    )
    args = parser.parse_args()

    if args.base % PAYLOAD_SIZE:
        parser.error("base address must be 256-byte aligned")

    firmware = args.input.read_bytes()
    block_count = (len(firmware) + PAYLOAD_SIZE - 1) // PAYLOAD_SIZE
    if not block_count:
        parser.error("input firmware is empty")

    flags = UF2_FLAG_FAMILY_ID_PRESENT if args.family_id is not None else 0
    family_id = args.family_id or 0
    with args.output.open("wb") as output:
        for block_no in range(block_count):
            payload = firmware[
                block_no * PAYLOAD_SIZE:(block_no + 1) * PAYLOAD_SIZE
            ].ljust(PAYLOAD_SIZE, b"\xff")
            header = struct.pack(
                "<IIIIIIII",
                UF2_MAGIC_START0,
                UF2_MAGIC_START1,
                flags,
                args.base + block_no * PAYLOAD_SIZE,
                PAYLOAD_SIZE,
                block_no,
                block_count,
                family_id,
            )
            output.write(header)
            output.write(payload)
            output.write(bytes(476 - PAYLOAD_SIZE))
            output.write(struct.pack("<I", UF2_MAGIC_END))


if __name__ == "__main__":
    main()
