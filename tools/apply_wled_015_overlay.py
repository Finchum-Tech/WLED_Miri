from pathlib import Path
import sys


root = Path(sys.argv[1])


def replace_once(path, old, new):
    source = path.read_text()
    if source.count(old) != 1:
        raise SystemExit(f"Expected one match for {old!r} in {path}")
    path.write_text(source.replace(old, new, 1))


const_h = root / "wled00" / "const.h"
replace_once(
    const_h,
    '#define USERMOD_ID_PIXELS_DICE_TRAY      54     //Usermod "pixels_dice_tray.h"',
    '#define USERMOD_ID_PIXELS_DICE_TRAY      54     //Usermod "pixels_dice_tray.h"\n'
    '#define USERMOD_ID_UI                    55     //Usermod UI overlay',
)

fcn_declare_h = root / "wled00" / "fcn_declare.h"
replace_once(
    fcn_declare_h,
    "void serveJsonError(AsyncWebServerRequest* request, uint16_t code, uint16_t error);",
    "void serveJsonError(AsyncWebServerRequest* request, uint16_t code, uint16_t error);\n"
    "void handleStaticContent(AsyncWebServerRequest *request, const String &path, int code, "
    "const String &contentType, const uint8_t *content, size_t len, bool gzip = true, "
    "uint16_t eTagSuffix = 0);\n"
    "bool captivePortal(AsyncWebServerRequest *request);",
)

usermods_list = root / "wled00" / "usermods_list.cpp"
replace_once(
    usermods_list,
    '#include "wled.h"\n',
    '#include "wled.h"\n\n#ifdef USERMOD_UI\n'
    '  #include "../usermods/usermod_ui/usermod_ui.h"\n'
    '#endif\n\n'
    '#ifdef USERMOD_PCA9634\n'
    '  #include "../usermods/usermod_v2_pca9634/usermod_v2_pca9634.cpp"\n'
    '#endif\n',
)
replace_once(
    usermods_list,
    '  //UsermodManager::add(new MyExampleUsermod());\n',
    '  //UsermodManager::add(new MyExampleUsermod());\n\n'
    '  #ifdef USERMOD_UI\n'
    '  UsermodManager::add(new UsermodUI());\n'
    '  #endif\n\n'
    '  #ifdef USERMOD_PCA9634\n'
    '  UsermodManager::add(new UsermodPCA9634());\n'
    '  #endif\n',
)

server_cpp = root / "wled00" / "wled_server.cpp"
replace_once(server_cpp, "static void handleStaticContent(", "void handleStaticContent(")
replace_once(server_cpp, ", bool gzip = true, uint16_t eTagSuffix = 0) {", ", bool gzip, uint16_t eTagSuffix) {")
replace_once(server_cpp, "static bool captivePortal(", "bool captivePortal(")
