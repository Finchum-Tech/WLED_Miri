#!/usr/bin/env python3
"""Regenerate generated_ui_bundle.h without a full PlatformIO build (dev only)."""
import gzip
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).resolve().parent))

USERMODS_DIR = ROOT / "usermods"
OUT_DIR = ROOT / "usermods" / "usermod_ui"
HEADER_PATH = OUT_DIR / "generated_ui_bundle.h"
HASH_PATH = OUT_DIR / ".bundle_inputs_hash"

# Match esp32dev_usermod_ui flags (override with env vars for allowlist testing)
CPPDEFINES = [
    ("USERMOD_UI",),
    ("USERMOD_UI_EXAMPLES",),
]
INCLUDE_RAW = os.environ.get("USERMOD_UI_INCLUDE", "").strip()

SKIP_DIR_NAMES = frozenset({"node_modules", "vendor", "third_party"})

RUNALL = """
window.WLEDUI._runAll = function (idoc) {
  Object.entries(window.WLEDUI._modules).forEach(function (entry) {
    var name = entry[0], init = entry[1];
    var container = idoc.querySelector('[data-usermod="' + name + '"]');
    if (!container) {
      container = idoc.createElement('div');
      container.setAttribute('data-usermod', name);
      idoc.body.appendChild(container);
    }
    try { init(container, idoc); } catch (e) { console.error('WLEDUI[' + name + '] failed:', e); }
  });
};
"""


def define_to_folder(define):
    if not define.startswith("USERMOD_"):
        return define.lower()
    name = define[8:].lower()
    direct = USERMODS_DIR / name
    prefixed = USERMODS_DIR / f"usermod_{name}"
    if prefixed.is_dir():
        return f"usermod_{name}"
    if direct.is_dir():
        return name
    return name


def enabled_usermod_dirs():
    dirs = {}
    for d in CPPDEFINES:
        define = d[0] if isinstance(d, tuple) else str(d)
        if not define.startswith("USERMOD_"):
            continue
        folder = define_to_folder(define)
        path = USERMODS_DIR / folder
        if path.is_dir():
            dirs[folder] = path
    return dirs


def should_skip_dir(dir_path: Path) -> bool:
    name = dir_path.name
    return name.startswith(".") or name in SKIP_DIR_NAMES


def collect_ui_js_recursive(usermod_dir: Path):
    found = []
    for root, dirs, files in os.walk(usermod_dir):
        root_path = Path(root)
        dirs[:] = sorted(d for d in dirs if not should_skip_dir(root_path / d))
        if "ui.js" in files:
            found.append(root_path / "ui.js")
    return sorted(found)


def module_key(usermod_name, ui_path: Path, usermod_dir: Path):
    rel = ui_path.relative_to(usermod_dir)
    if rel.parent == Path("."):
        return usermod_name
    suffix = str(rel.parent).replace("\\", "/").replace("/", "__")
    return f"{usermod_name}__{suffix}"


def include_implied_usermod_dirs(enabled_dirs, include_raw):
    dirs = dict(enabled_dirs)
    if not include_raw:
        return dirs
    for entry in include_raw.split(","):
        entry = entry.strip().replace("\\", "/").strip("'\"")
        if not entry:
            continue
        folder_name = entry.split("/")[0]
        candidate = USERMODS_DIR / folder_name
        if candidate.is_dir():
            dirs[folder_name] = candidate
    return dirs


def discover_explicit(enabled_dirs, include_raw):
    if not include_raw:
        return None
    dirs = include_implied_usermod_dirs(enabled_dirs, include_raw)
    modules = []
    for entry in include_raw.split(","):
        entry = entry.strip().replace("\\", "/").strip("'\"")
        if not entry:
            continue
        usermod = entry.split("/")[0]
        if usermod not in dirs:
            continue
        ui_path = USERMODS_DIR / entry
        if not ui_path.is_file():
            continue
        usermod_dir = dirs[usermod]
        key = module_key(usermod, ui_path, usermod_dir)
        modules.append((key, ui_path))
    return modules


def discover_modules():
    enabled = enabled_usermod_dirs()
    if INCLUDE_RAW:
        return discover_explicit(enabled, INCLUDE_RAW)
    modules = []
    for usermod_name in sorted(enabled):
        usermod_dir = enabled[usermod_name]
        for ui_path in collect_ui_js_recursive(usermod_dir):
            key = module_key(usermod_name, ui_path, usermod_dir)
            modules.append((key, ui_path))
    return modules


def syntax_check(path):
    subprocess.run(["node", "--check", str(path)], check=True, capture_output=True)


def minify_js(source):
    script = r"""
const { minify } = require('terser');
let data = '';
process.stdin.on('data', c => { data += c; });
process.stdin.on('end', async () => {
  try {
    const r = await minify(data, { compress: true, mangle: false });
    process.stdout.write(r.code || data);
  } catch (e) {
    process.stdout.write(data);
  }
});
"""
    result = subprocess.run(
        ["node", "-e", script],
        input=source,
        capture_output=True,
        text=True,
        cwd=str(ROOT),
        check=True,
    )
    return result.stdout or source


def wrap_module(name, body):
    safe = json.dumps(name)
    return (
        f"window.WLEDUI._modules[{safe}] = (function () {{\n"
        f"{body}\n"
        f"  return init;\n"
        f"}})();\n"
    )


def build_bundle_js(modules):
    parts = ["window.WLEDUI = window.WLEDUI || { _modules: {} };\n"]
    for name, path in modules:
        syntax_check(path)
        body = path.read_text(encoding="utf-8")
        parts.append(wrap_module(name, body))
    parts.append(RUNALL)
    return minify_js("".join(parts))


def format_progmem(data):
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    return "\n".join(lines)


def main():
    modules = discover_modules()
    bundle_js = build_bundle_js(modules)
    gz_data = gzip.compress(bundle_js.encode("utf-8"), compresslevel=9)
    content = (
        "#pragma once\n"
        "// Auto-generated by tools/bundle_ui.py - do not edit.\n"
        f"const uint8_t UI_BUNDLE[] PROGMEM = {{\n"
        f"{format_progmem(gz_data)}\n"
        f"}};\n"
        f"const size_t UI_BUNDLE_LENGTH = {len(gz_data)};\n"
        f"// Uncompressed bundle size: {len(bundle_js)} bytes\n"
    )
    HEADER_PATH.write_text(content, encoding="utf-8")
    names = ", ".join(m[0] for m in modules) or "(empty)"
    print(f"regen_bundle_dev: bundled [{names}] - {len(bundle_js)} B raw, {len(gz_data)} B gzip")


if __name__ == "__main__":
    main()
