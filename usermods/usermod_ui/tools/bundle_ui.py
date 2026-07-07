try:
    Import("env")
except NameError:
    pass  # injected when run via tools/bundle_entry.py exec()
import gzip
import hashlib
import json
import os
import subprocess
from pathlib import Path

PROJECT_DIR = Path(env["PROJECT_DIR"])
USERMODS_DIR = PROJECT_DIR / "usermods"
OUT_DIR = PROJECT_DIR / "usermods" / "usermod_ui"
HEADER_PATH = OUT_DIR / "generated_ui_bundle.h"
HASH_PATH = OUT_DIR / ".bundle_inputs_hash"

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


def flatten_define_flags(flags):
    """Parse -D NAME[=VALUE] tokens from PIO build flag lists."""
    if flags is None:
        return []
    tokens = []
    if isinstance(flags, str):
        tokens = flags.split()
    elif isinstance(flags, (list, tuple)):
        for item in flags:
            tokens.extend(str(item).split())
    else:
        return []

    defines = []
    i = 0
    while i < len(tokens):
        tok = tokens[i]
        if tok == "-D" and i + 1 < len(tokens):
            rest = tokens[i + 1]
            i += 2
        elif tok.startswith("-D"):
            rest = tok[2:]
            i += 1
        else:
            i += 1
            continue
        if rest.startswith('"') and rest.endswith('"'):
            rest = rest[1:-1]
        name, _, value = rest.partition("=")
        defines.append((name, value if _ else None))
    return defines


def ini_build_flags():
    try:
        from platformio.project.config import ProjectConfig

        pioenv = env.get("PIOENV") or os.environ.get("PIOENV", "")
        if not pioenv:
            return []
        config = ProjectConfig(str(PROJECT_DIR / "platformio.ini"))
        return flatten_define_flags(config.get(f"env:{pioenv}", "build_flags"))
    except Exception:
        return []


def cpp_defines():
    out = []
    seen = set()

    def add(name):
        if name and name not in seen:
            seen.add(name)
            out.append(name)

    for d in env.get("CPPDEFINES", []):
        add(d[0] if isinstance(d, tuple) else str(d))

    for key in ("BUILD_FLAGS", "CCFLAGS", "CXXFLAGS", "CPPFLAGS"):
        for name, _value in flatten_define_flags(env.get(key)):
            add(name)

    for name, _value in ini_build_flags():
        add(name)

    return out


def cpp_define_value(name):
    for d in env.get("CPPDEFINES", []):
        if isinstance(d, tuple) and d[0] == name:
            val = d[1]
            if isinstance(val, str):
                return val.strip('"')
            return str(val)

    for key in ("BUILD_FLAGS", "CCFLAGS", "CXXFLAGS", "CPPFLAGS"):
        for define_name, value in flatten_define_flags(env.get(key)):
            if define_name == name and value is not None:
                return value.strip('"')

    for define_name, value in ini_build_flags():
        if define_name == name and value is not None:
            return value.strip('"')

    return None


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
    for define in sorted(cpp_defines()):
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


def discover_explicit(enabled_dirs):
    include_raw = cpp_define_value("USERMOD_UI_INCLUDE")
    if not include_raw:
        return None
    modules = []
    for entry in include_raw.split(","):
        entry = entry.strip().replace("\\", "/")
        if not entry:
            continue
        parts = entry.split("/")
        if not parts:
            continue
        usermod = parts[0].lower()
        if usermod not in enabled_dirs:
            continue
        ui_path = USERMODS_DIR / entry
        if not ui_path.is_file():
            continue
        usermod_dir = enabled_dirs[usermod]
        try:
            ui_path.resolve().relative_to(usermod_dir.resolve())
        except ValueError:
            continue
        key = module_key(usermod, ui_path, usermod_dir)
        modules.append((key, ui_path))
    return modules


def discover_default(enabled_dirs):
    modules = []
    for usermod_name in sorted(enabled_dirs):
        usermod_dir = enabled_dirs[usermod_name]
        for ui_path in collect_ui_js_recursive(usermod_dir):
            key = module_key(usermod_name, ui_path, usermod_dir)
            modules.append((key, ui_path))
    return modules


def discover_modules():
    enabled_dirs = enabled_usermod_dirs()
    explicit = discover_explicit(enabled_dirs)
    if explicit is not None:
        return explicit
    return discover_default(enabled_dirs)


def syntax_check(path):
    for candidate in ("node", env.get("PYTHONEXE", "node")):
        try:
            subprocess.run(
                [candidate, "--check", str(path)],
                check=True,
                capture_output=True,
                text=True,
            )
            return
        except (subprocess.CalledProcessError, FileNotFoundError, TypeError):
            continue
    raise RuntimeError("bundle_ui.py: node not found - required for ui.js syntax check")


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
    try:
        result = subprocess.run(
            ["node", "-e", script],
            input=source,
            capture_output=True,
            text=True,
            cwd=str(PROJECT_DIR),
            check=True,
        )
        return result.stdout or source
    except (subprocess.CalledProcessError, FileNotFoundError):
        return source


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


def inputs_hash(modules, bundle_js):
    h = hashlib.sha256()
    h.update(bundle_js.encode("utf-8"))
    for name, path in modules:
        h.update(name.encode())
        h.update(str(path).encode())
        h.update(path.read_bytes())
    for d in sorted(cpp_defines()):
        if d.startswith("USERMOD_"):
            h.update(d.encode())
    inc = cpp_define_value("USERMOD_UI_INCLUDE")
    if inc:
        h.update(inc.encode())
    return h.hexdigest()


def write_header(gz_data, raw_len):
    content = (
        "#pragma once\n"
        "// Auto-generated by tools/bundle_ui.py - do not edit.\n"
        f"const uint8_t UI_BUNDLE[] PROGMEM = {{\n"
        f"{format_progmem(gz_data)}\n"
        f"}};\n"
        f"const size_t UI_BUNDLE_LENGTH = {len(gz_data)};\n"
        f"// Uncompressed bundle size: {raw_len} bytes\n"
    )
    HEADER_PATH.write_text(content, encoding="utf-8")


def generate_bundle():
    modules = discover_modules()
    if not modules:
        defines = sorted(cpp_defines())
        usermod_defines = [d for d in defines if d.startswith("USERMOD_")]
        pioenv = env.get("PIOENV") or os.environ.get("PIOENV", "(unset)")
        examples_dir = USERMODS_DIR / "usermod_ui_examples"
        has_examples = examples_dir.is_dir() and any(examples_dir.rglob("ui.js"))
        msg = (
            "bundle_ui.py: ERROR - no ui.js modules discovered; "
            f"PIOENV={pioenv}; USERMOD_* defines: {usermod_defines or '(none)'}"
        )
        if has_examples and "USERMOD_UI_EXAMPLES" in usermod_defines:
            raise SystemExit(msg + " (stale usermods/ui_examples/ folder can cause this)")
        print("bundle_ui.py: WARNING - " + msg.split("ERROR - ", 1)[-1])

    bundle_js = build_bundle_js(modules)
    digest = inputs_hash(modules, bundle_js)

    if HASH_PATH.is_file() and HASH_PATH.read_text(encoding="utf-8").strip() == digest:
        print("bundle_ui.py: unchanged - skipping regeneration")
        return

    gz_data = gzip.compress(bundle_js.encode("utf-8"), compresslevel=9)
    write_header(gz_data, len(bundle_js))
    HASH_PATH.write_text(digest, encoding="utf-8")
    names = ", ".join(m[0] for m in modules) or "(empty)"
    print(
        f"bundle_ui.py: bundled [{names}] - "
        f"{len(bundle_js)} B raw, {len(gz_data)} B gzip -> generated_ui_bundle.h"
    )


generate_bundle()
