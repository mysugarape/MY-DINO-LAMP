#ifndef USERMOD_DINO_KILLSWITCH_H
#define USERMOD_DINO_KILLSWITCH_H

#include "wled.h"

/*
 * ============================================================
 *  Dino-Lampe – WLAN-Kill-Switch Usermod (Testversion ohne LED-Code)
 * ============================================================
 *  Nur noch die reine Taster-Logik: haelt man beide Taster
 *  DINO_HOLD_MS lang gedrueckt, wird WLAN aus- bzw. wieder
 *  eingeschaltet. Kein Zugriff mehr auf die Onboard-LED oder
 *  den WS2812-Ring.
 * ============================================================
 */

// ======================= EINSTELLUNGEN ========================
#define DINO_BTN_PIN_1   4     // GPIO4  / D2  - Button "An/Aus"
#define DINO_BTN_PIN_2   14    // GPIO14 / D5  - Button "Dimmen"
#define DINO_HOLD_MS     3000  // Haltezeit der Kombi in Millisekunden
// ===============================================================

class UsermodDinoKillswitch : public Usermod {
  private:
    bool     enabled  = true;
    int8_t   btnPin1  = DINO_BTN_PIN_1;
    int8_t   btnPin2  = DINO_BTN_PIN_2;
    uint16_t holdMs   = DINO_HOLD_MS;

    unsigned long comboStart        = 0;
    bool          comboActive       = false;
    bool          triggeredThisHold = false;

    void updatePinModes() {
      if (btnPin1 >= 0) pinMode(btnPin1, INPUT_PULLUP);
      if (btnPin2 >= 0) pinMode(btnPin2, INPUT_PULLUP);
    }

  public:
    void setup() override {
      updatePinModes();
      DEBUG_PRINTF_P(PSTR("[DinoKillswitch] setup: btnPin1=%d btnPin2=%d holdMs=%d\n"), btnPin1, btnPin2, holdMs);
    }

    void loop() override {
      if (!enabled) return;
      if (btnPin1 < 0 || btnPin2 < 0) return;

      bool bothPressed = (digitalRead(btnPin1) == LOW) && (digitalRead(btnPin2) == LOW);

      if (bothPressed) {
        if (!comboActive) {
          comboActive       = true;
          comboStart        = millis();
          triggeredThisHold = false;
        } else if (!triggeredThisHold && millis() - comboStart >= holdMs) {
          triggeredThisHold = true;
          toggleWifi();
        }
      } else {
        comboActive = false;
      }
    }

    void toggleWifi() {
      if (WiFi.getMode() != WIFI_OFF) {
        DEBUG_PRINTLN(F("[DinoKillswitch] WLAN wird deaktiviert"));
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
#ifdef ESP8266
        WiFi.forceSleepBegin();
#endif
      } else {
        DEBUG_PRINTLN(F("[DinoKillswitch] WLAN wird reaktiviert (Neustart)"));
        delay(400);
        ESP.restart();
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject("DinoKillswitch");
      top["enabled"] = enabled;
      top["btnPin1"] = btnPin1;
      top["btnPin2"] = btnPin2;
      top["holdMs"]  = holdMs;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root["DinoKillswitch"];
      bool ok = !top.isNull();
      ok &= getJsonValue(top["enabled"], enabled, true);
      ok &= getJsonValue(top["btnPin1"], btnPin1, (int8_t)DINO_BTN_PIN_1);
      ok &= getJsonValue(top["btnPin2"], btnPin2, (int8_t)DINO_BTN_PIN_2);
      ok &= getJsonValue(top["holdMs"],  holdMs,  (uint16_t)DINO_HOLD_MS);

      if (ok) updatePinModes();

      return ok;
    }

    void addToJsonInfo(JsonObject& root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray infoArr = user.createNestedArray("WLAN Killswitch");
      infoArr.add(enabled ? "aktiv" : "deaktiviert");
    }
};

static UsermodDinoKillswitch dino_killswitch;
REGISTER_USERMOD(dino_killswitch);

#endif