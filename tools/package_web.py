#!/usr/bin/env python3
"""Package the Emscripten output with content-addressed URLs; no extra dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--raylib-license", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    site = args.build_dir / "site"
    site.mkdir(parents=True, exist_ok=True)
    files = {}
    for suffix in ("js", "wasm", "data"):
        original = f"boat_study.{suffix}"
        content = (args.build_dir / original).read_bytes()
        digest = hashlib.sha256(content).hexdigest()[:16]
        name = f"boat_study-{digest}.{suffix}"
        (site / name).write_bytes(content)
        files[original] = name
    template = (root / "web/index.html").read_text()
    (site / "index.html").write_text(template.replace("@WEB_FILES@", json.dumps(files)))
    (site / ".nojekyll").touch()
    shutil.copyfile(args.raylib_license, site / "raylib-LICENSE.txt")
    # Remove outputs from earlier builds only, never unrelated files.
    for pattern in ("boat_study-*.js", "boat_study-*.wasm", "boat_study-*.data"):
        for path in site.glob(pattern):
            if path.name not in files.values():
                path.unlink()
    total = sum((site / name).stat().st_size for name in files.values())
    print(f"Web build: {site} ({total:,} bytes before HTTP compression)")


if __name__ == "__main__":
    main()
