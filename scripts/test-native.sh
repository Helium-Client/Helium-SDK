#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${1:-"$ROOT/build"}
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release
ctest --test-dir "$BUILD_DIR" --build-config Release --output-on-failure
python3 - "$BUILD_DIR/package/dev.helium.cpp-example/helium-plugin.json" "$ROOT/manifest/helium-plugin.schema.json" <<'PY'
import json
import pathlib
import re
import sys
manifest = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
schema = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
assert manifest["api_version"] == schema["properties"]["api_version"]["const"]
assert manifest["id"] == "dev.helium.cpp-example"
assert pathlib.Path(sys.argv[1]).parent.joinpath(manifest["library"]).is_file()
assert re.fullmatch(schema["properties"]["id"]["pattern"], manifest["id"])
print("manifest and library package checks passed")
PY
