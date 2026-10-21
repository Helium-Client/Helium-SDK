#!/usr/bin/env python3
import json
import platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIBRARIES = {
    "Windows": "helium_example_rust_plugin.dll",
    "Linux": "libhelium_example_rust_plugin.so",
    "Darwin": "libhelium_example_rust_plugin.dylib",
}


def main() -> int:
    os_name = platform.system()
    library_name = LIBRARIES.get(os_name)
    if library_name is None:
        print(f"Unsupported operating system: {os_name}", file=sys.stderr)
        return 2

    subprocess.run(
        ["cargo", "build", "-p", "helium-example-rust-plugin", "--release"],
        cwd=ROOT,
        check=True,
    )

    built_library = ROOT / "target" / "release" / library_name
    if not built_library.is_file():
        print(f"Expected build output not found: {built_library}", file=sys.stderr)
        return 1

    package_dir = ROOT / "build" / "package" / "dev.helium.rust-example"
    package_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built_library, package_dir / library_name)

    manifest = {
        "id": "dev.helium.rust-example",
        "name": "Helium Rust Example",
        "version": "1.0.0",
        "api_version": 1,
        "library": library_name,
    }
    (package_dir / "helium-plugin.json").write_text(
        json.dumps(manifest, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Rust plugin package created: {package_dir}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
