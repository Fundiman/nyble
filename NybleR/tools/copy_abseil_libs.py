#!/usr/bin/env python3
"""Install Tint's locally built Abseil static libraries next to Tint's libs.

Tint's CMake install rules export the tint_* libraries but not the Abseil
libraries they depend on. Clients of the Tint bridge need both sets, so copy
the already-built Abseil archives into the same prefix instead of rebuilding
or downloading anything.
"""

import argparse
import pathlib
import shutil


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("absl_root", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()

    args.destination.mkdir(parents=True, exist_ok=True)
    copied = 0
    for source in sorted(args.absl_root.rglob("*.lib")):
        shutil.copy2(source, args.destination / source.name)
        copied += 1
    for source in sorted(args.absl_root.rglob("*.a")):
        shutil.copy2(source, args.destination / source.name)
        copied += 1
    print(f"installed {copied} Abseil libraries into {args.destination}")


if __name__ == "__main__":
    main()
