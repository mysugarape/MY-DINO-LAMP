#pragma once
#include "wled.h"

/*
 * ============================================================
 *  Dino-Lampe – WLAN-Kill-Switch Usermod
 * ============================================================
 *  Haelt man die beiden unten definierten Taster gleichzeitig
 *  fuer DINO_HOLD_MS Millisekunden gedrueckt, wird das WLAN
 *  abgeschaltet. Die gleiche Kombination erneut gehalten
 *  startet den Wemos neu - WLED verbindet sich beim Boot
 *  dann ganz normal wieder mit dem gespeicherten WLAN.
 *
 *  Die Buttons bleiben parallel dazu ganz normal ueber die
 *  WLED-eigene Button-Konfiguration nutzbar - dieses Usermod
 *  liest die Pins nur zusaetzlich per digitalRead() mit.
 * ============================================================
 */

// ======================= EINSTELLUNGEN ========================
#define DINO_BTN_PIN_1   4     // GPIO4  / D2  - Button "An/Aus"
#define DINO_BTN_PIN_2   14    // GPIO14 / D5  - Button "Dimmen"
#define DINO_HOLD_MS     3000  // Haltezeit der Kombi in Millisekunden
#define DINO_NUM_LEDS    12    // Anzahl LEDs im Ring, fuer das Blink-Feedback
#define DINO_ONBOARD_LED LED_BUILTIN // GPIO2/D4, fest verbaute blaue Onboard-LED (aktiv LOW)
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

      // Onboard-LED auf dem Wemos D1 Mini ist aktiv LOW (LOW = an, HIGH = aus)
      // Wichtig: GPIO2/D4 ist auch der Boot-Pin (TXD1). Nach dem Initialisieren dauerhaft deaktivieren.
      pinMode(DINO_ONBOARD_LED, OUTPUT);
      digitalWrite(DINO_ONBOARD_LED, HIGH);

      DEBUG_PRINTF_P(PSTR("[DinoKillswitch] setup: btnPin1=%d btnPin2=%d holdMs=%d\n"), btnPin1, btnPin2, holdMs);
    }

    void loop() override {
      if (!enabled) return;

      // Sicherheitscheck: Verhindert Fehlausloesungen, wenn Taster/GND noch nicht verdrahtet sind oder auf -1 stehen
      if (btnPin1 < 0 || btnPin2 < 0) return;

      // Die Pins verwenden INPUT_PULLUP. Ohne externe Beschaltung lesen sie HIGH.
      // Erst wenn BEIDE Taster gleichzeitig gegen GND gezogen werden (LOW), zaehlt der Timer.
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
        flashFeedback(0xFF0000); // Rot = WLAN geht aus
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
#ifdef ESP8266
        WiFi.forceSleepBegin();
#endif
      } else {
        DEBUG_PRINTLN(F("[DinoKillswitch] WLAN wird reaktiviert (Neustart)"));
        flashFeedback(0x00FF00); // Gruen = WLAN geht wieder an
        delay(400);
        ESP.restart();
      }
    }

    void flashFeedback(uint32_t color) {
      uint16_t n = strip.getLengthTotal();
      if (n > DINO_NUM_LEDS) n = DINO_NUM_LEDS;
      uint32_t original[DINO_NUM_LEDS];
      for (uint16_t i = 0; i < n; i++) original[i] = strip.getPixelColor(i);

      for (uint8_t b = 0; b < 2; b++) {
        for (uint16_t i = 0; i < n; i++) strip.setPixelColor(i, color);
        strip.show();
        delay(150);
        for (uint16_t i = 0; i < n; i++) strip.setPixelColor(i, 0);
        strip.show();
        delay(150);
      }

      for (uint16_t i = 0; i < n; i++) strip.setPixelColor(i, original[i]);
      strip.show();
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

      // Pins neu konfigurieren, falls sie ueber das WLED Web-Interface geaendert wurden
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