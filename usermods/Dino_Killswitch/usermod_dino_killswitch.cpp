#ifndef USERMOD_DINO_KILLSWITCH_H
#define USERMOD_DINO_KILLSWITCH_H

#include "wled.h"

/*
 * ============================================================
 *  Dino-Lampe – WLAN-Kill-Switch Usermod
 * ============================================================
 *  Haelt man beide Taster DINO_HOLD_MS lang gedrueckt, wird
 *  das WLAN aus- bzw. wieder eingeschaltet.
 *
 *  WICHTIG - einmaliger manueller Schritt in der WLED-Oberflaeche:
 *  Config -> LED Preferences -> Button -> Button 0 (GPIO4/D2) auf
 *  Pin -1 (deaktiviert) stellen! Sonst kollidiert WLEDs eigene
 *  "5 Sekunden halten = AP neu oeffnen"-Logik auf Button 0 mit
 *  unserer Kombi. Button 3/GPIO14 bleibt normal in WLED konfiguriert,
 *  die hat dieses Sonderverhalten nicht.
 *
 *  Dieses Usermod uebernimmt den Kurzdruck-An/Aus fuer GPIO4 selbst
 *  (repliziert WLEDs eigene shortPressAction fuer Button 0), damit
 *  die Taste trotzdem normal funktioniert.
 * ============================================================
 */

// ======================= EINSTELLUNGEN ========================
#define DINO_BTN_PIN_1        4     // GPIO4  / D2  - Button "An/Aus" (WLED-Button 0 hier deaktiviert)
#define DINO_BTN_PIN_2        14    // GPIO14 / D5  - Button "Dimmen" (bleibt normal in WLED konfiguriert)
#define DINO_HOLD_MS          3000  // Haltezeit der Kombi in Millisekunden
#define DINO_SHORT_PRESS_MS   600   // max. Dauer fuer Einzel-Kurzdruck auf Pin1 (An/Aus)
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

    // Fuer den selbst uebernommenen Kurzdruck auf Pin1 (An/Aus)
    bool          btn1WasPressed    = false;
    unsigned long btn1PressStart    = 0;

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

      bool pin1Pressed = (digitalRead(btnPin1) == LOW);
      bool pin2Pressed = (digitalRead(btnPin2) == LOW);
      bool bothPressed = pin1Pressed && pin2Pressed;

      // --- Kombi-Erkennung fuer den WLAN-Kill-Switch ---
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

      // --- Einzel-Kurzdruck auf Pin1 = An/Aus (ersetzt WLEDs eigenes Button 0) ---
      if (pin1Pressed) {
        if (!btn1WasPressed) {
          btn1WasPressed = true;
          btn1PressStart = millis();
        }
      } else {
        if (btn1WasPressed) {
          unsigned long dur = millis() - btn1PressStart;
          btn1WasPressed = false;
          // Nur toggeln, wenn es ein kurzer Einzeldruck war (kein Teil der WLAN-Kombi)
          if (!triggeredThisHold && dur >= 50 && dur < DINO_SHORT_PRESS_MS) {
            toggleOnOff();
            stateUpdated(CALL_MODE_BUTTON);
          }
        }
      }
    }

    void toggleWifi() {
      if (WiFi.getMode() != WIFI_OFF) {
        DEBUG_PRINTLN(F("[DinoKillswitch] WLAN wird deaktiviert"));
        WiFi.disconnect(true);
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