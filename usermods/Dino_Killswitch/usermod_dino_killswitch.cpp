#ifndef USERMOD_DINO_KILLSWITCH_H
#define USERMOD_DINO_KILLSWITCH_H

#include "wled.h"

/*
 * ============================================================
 *  Dino-Lampe – WLAN-Kill-Switch Usermod
 * ============================================================
 *  Haelt man beide Taster DINO_HOLD_MS lang gedrueckt, wird
 *  das WLAN aus- bzw. wieder eingeschaltet. Der Ring blinkt
 *  dabei kurz zur Rueckmeldung (nicht-blockierend!).
 *
 *  WICHTIG: Solange wifiKilled aktiv ist, wird bei JEDEM loop()
 *  geprueft, ob WLED von sich aus wieder WiFi aktiviert hat
 *  (das passiert automatisch ueber WLEDs eigene handleConnection()-
 *  Reconnect-Logik) - und falls ja, sofort wieder abgeschaltet.
 *
 *  WICHTIG - einmaliger manueller Schritt in der WLED-Oberflaeche:
 *  Config -> LED Preferences -> Button -> Button 0 (GPIO4/D2) auf
 *  Pin -1 (deaktiviert) stellen! Button 3/GPIO14 bleibt normal
 *  in WLED konfiguriert.
 * ============================================================
 */

// ======================= EINSTELLUNGEN ========================
#define DINO_BTN_PIN_1        4     // GPIO4  / D2  - Button "An/Aus" (WLED-Button 0 hier deaktiviert)
#define DINO_BTN_PIN_2        14    // GPIO14 / D5  - Button "Dimmen" (bleibt normal in WLED konfiguriert)
#define DINO_HOLD_MS          3000  // Haltezeit der Kombi in Millisekunden
#define DINO_SHORT_PRESS_MS   600   // max. Dauer fuer Einzel-Kurzdruck auf Pin1 (An/Aus)
#define DINO_FLASH_PHASE_MS   200   // Dauer je Blink-Phase (an/aus)
#define DINO_FLASH_COUNT      4     // Anzahl Phasen = 2x an + 2x aus (2 Blitze)
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

    // Haelt fest, ob das WLAN gerade per Kill-Switch deaktiviert sein SOLL
    bool          wifiKilled        = false;

    bool          btn1WasPressed    = false;
    unsigned long btn1PressStart    = 0;

    bool          flashActive       = false;
    bool          flashOn           = false;
    uint8_t       flashPhasesLeft   = 0;
    unsigned long flashNextPhase    = 0;
    uint32_t      flashColor        = 0;
    uint8_t       savedMode         = FX_MODE_STATIC;
    uint32_t      savedColor        = 0;

    bool          pendingRestart    = false;
    unsigned long restartAt         = 0;

    void updatePinModes() {
      if (btnPin1 >= 0) pinMode(btnPin1, INPUT_PULLUP);
      if (btnPin2 >= 0) pinMode(btnPin2, INPUT_PULLUP);
    }

    void enforceWifiOff() {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
#ifdef ESP8266
      WiFi.forceSleepBegin();
#endif
    }

    void startFlash(uint32_t color) {
      Segment &seg = strip.getMainSegment();
      savedMode  = seg.mode;
      savedColor = seg.colors[0];

      flashColor      = color;
      flashPhasesLeft = DINO_FLASH_COUNT;
      flashOn         = false;
      flashActive     = true;
      flashNextPhase  = millis();
    }

    void handleFlash() {
      if (!flashActive) return;
      if (millis() < flashNextPhase) return;

      Segment &seg = strip.getMainSegment();

      if (flashPhasesLeft == 0) {
        seg.setMode(savedMode);
        seg.setColor(0, savedColor);
        flashActive = false;
        return;
      }

      flashOn = !flashOn;
      seg.setMode(FX_MODE_STATIC);
      seg.setColor(0, flashOn ? flashColor : 0x000000);

      flashPhasesLeft--;
      flashNextPhase = millis() + DINO_FLASH_PHASE_MS;
    }

  public:
    void setup() override {
      updatePinModes();
      DEBUG_PRINTF_P(PSTR("[DinoKillswitch] setup: btnPin1=%d btnPin2=%d holdMs=%d\n"), btnPin1, btnPin2, holdMs);
    }

    void loop() override {
      // Staendig durchsetzen: solange WLAN per Kill-Switch aus sein soll,
      // WLEDs eigene automatische Reconnect-Versuche sofort zunichtemachen.
      if (wifiKilled && WiFi.getMode() != WIFI_OFF) {
        enforceWifiOff();
      }

      handleFlash();

      if (pendingRestart && millis() >= restartAt) {
        pendingRestart = false;
        ESP.restart();
        return;
      }

      if (!enabled) return;
      if (btnPin1 < 0 || btnPin2 < 0) return;

      bool pin1Pressed = (digitalRead(btnPin1) == LOW);
      bool pin2Pressed = (digitalRead(btnPin2) == LOW);
      bool bothPressed = pin1Pressed && pin2Pressed;

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

      if (pin1Pressed) {
        if (!btn1WasPressed) {
          btn1WasPressed = true;
          btn1PressStart = millis();
        }
      } else {
        if (btn1WasPressed) {
          unsigned long dur = millis() - btn1PressStart;
          btn1WasPressed = false;
          if (!triggeredThisHold && dur >= 50 && dur < DINO_SHORT_PRESS_MS) {
            toggleOnOff();
            stateUpdated(CALL_MODE_BUTTON);
          }
        }
      }
    }

    void toggleWifi() {
      if (!wifiKilled) {
        DEBUG_PRINTLN(F("[DinoKillswitch] WLAN wird deaktiviert"));
        wifiKilled = true;
        startFlash(0xFF0000); // rot = WLAN geht aus
        enforceWifiOff();
      } else {
        DEBUG_PRINTLN(F("[DinoKillswitch] WLAN wird reaktiviert (Neustart)"));
        wifiKilled = false;
        startFlash(0x00FF00); // gruen = WLAN geht wieder an
        pendingRestart = true;
        restartAt = millis() + (DINO_FLASH_COUNT * DINO_FLASH_PHASE_MS) + 200;
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
      infoArr.add(enabled ? (wifiKilled ? "WLAN AUS" : "WLAN AN") : "deaktiviert");
    }
};

static UsermodDinoKillswitch dino_killswitch;
REGISTER_USERMOD(dino_killswitch);

#endif