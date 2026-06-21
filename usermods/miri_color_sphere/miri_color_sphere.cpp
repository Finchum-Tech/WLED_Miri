#include "wled.h"

#ifdef USERMOD_MIRI_COLOR_SPHERE

#include "sphere_html.h"

class MiriColorSphereUsermod : public Usermod {
 public:
  void setup() override {
    server.on(F("/sphere"), HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send_P(200, PSTR("text/html"), MIRI_SPHERE_HTML);
    });
  }

  void loop() override {}

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray colorSphere = user.createNestedArray(F("Color Sphere"));
    colorSphere.add(F("/sphere"));
    colorSphere.add(F("url"));
  }
};

static MiriColorSphereUsermod miriColorSphereUsermod;
REGISTER_USERMOD(miriColorSphereUsermod);

#endif