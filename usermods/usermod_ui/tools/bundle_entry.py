# Delegates to the repo-source bundler so a stale file:// copy under .pio/libdeps
# never runs outdated discovery logic. library.json extraScript points here.
import os
from pathlib import Path

Import = globals().get("Import")
if Import is None:
    def Import(_name):
        return None

    env = {
        "PROJECT_DIR": Path(os.getcwd()),
        "PIOENV": os.environ.get("PIOENV", ""),
        "CPPDEFINES": [("USERMOD_UI", None)],
    }
else:
    Import("env")

_SOURCE = Path(env["PROJECT_DIR"]) / "usermods" / "usermod_ui" / "tools" / "bundle_ui.py"
exec(compile(_SOURCE.read_text(encoding="utf-8"), str(_SOURCE), "exec"), {"env": env, "Import": Import})
