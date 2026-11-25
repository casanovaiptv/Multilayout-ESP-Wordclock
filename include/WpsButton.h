#pragma once

#include "WordClockState.h"
#include <atomic>

#ifdef ESP8266
#include <coredecls.h>
extern "C" {
#include <user_interface.h>
}
#elif defined(ESP32)
#include <esp_wifi.h>
#include <esp_wps.h>
#endif

inline bool wpsPinAllowed(uint8_t pin) {
#if defined(ESP8266)
    const bool usable = pin == 4 || pin == 5 || (pin >= 12 && pin <= 14);
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    const bool usable = pin == 1 || pin == 3 || (pin >= 4 && pin <= 7) || pin == 10;
#else
    const bool usable = pin == 4 || pin == 13 || pin == 14 ||
                        (pin >= 16 && pin <= 19) || pin == 21 || pin == 22 ||
                        pin == 23 || pin == 25 || pin == 26 || pin == 27 ||
                        pin == 32 || pin == 33;
#endif
    return usable && pin != G.hardwarePins.led &&
           pin != G.hardwarePins.powerButton && pin != G.hardwarePins.modeButton &&
           pin != G.hardwarePins.speedButton && pin != G.i2cSdaPin &&
           pin != G.i2cSclPin;
}

class WpsButton {
public:
    enum class State : uint8_t { Idle, Pairing, Connecting, Success, Failed };

    void begin() {
#ifdef ESP8266
        // The NONOS WPS implementation uses the SDK's extra 4 KB RAM.
        disable_extra4k_at_link_time();
#else
        WiFi.onEvent(onEvent);
#endif
    }

    State update() {
        const uint32_t now = millis();
        if (state == State::Success || state == State::Failed) {
            if (now - since >= 2000) state = State::Idle;
            return state;
        }
        if (state == State::Pairing) {
            const Event event = takeResult();
            if (event == Event::Paired) {
                stopPairing();
                state = State::Connecting;
                since = now;
                if (!connectProfile()) fail(now);
            } else if (event == Event::Error || now - since >= 120000) {
                stopPairing();
                fail(now);
            }
            return state;
        }
        if (state == State::Connecting) {
            if (WiFi.status() == WL_CONNECTED) {
                if (saveProfile()) {
                    WiFi.setAutoReconnect(previousAutoReconnect);
                    state = State::Success;
                    since = now;
                } else {
                    fail(now);
                }
            } else if (now - since >= 20000) {
                fail(now);
            }
            return state;
        }

        const uint8_t pin = hardware.has(hardwareSettings::WpsEnabled) &&
                                    wpsPinAllowed(hardware.wpsPin)
                                ? hardware.wpsPin : HARDWARE_PIN_DISABLED;
        if (pin != activePin) {
            activePin = pin;
            pressed = false;
            consumed = false;
            if (pin != HARDWARE_PIN_DISABLED) pinMode(pin, INPUT_PULLUP);
        }
        if (pin == HARDWARE_PIN_DISABLED || digitalRead(pin) != LOW) {
            pressed = false;
            consumed = false;
        } else if (!pressed) {
            pressed = true;
            pressedAt = now;
        } else if (!consumed && now - pressedAt >= 3000) {
            consumed = true;
            // Stop the portal before touching the station mode or credentials.
            if (wifiManager.getConfigPortalActive()) wifiManager.stopConfigPortal();
            start(now);
        }
        return state;
    }

    bool needsPortal() {
        const bool requested = openPortal;
        openPortal = false;
        return requested;
    }

private:
    enum class Event : uint32_t { None, Paired, Error };
#ifdef ESP8266
    static volatile Event result;
#else
    static std::atomic<Event> result;
#endif

    static void setResult(Event event) {
#ifdef ESP8266
        result = event;
#else
        result.store(event);
#endif
    }

    static Event takeResult() {
#ifdef ESP8266
        // SDK callbacks run between Arduino loop iterations or on yield().
        const Event event = result;
        result = Event::None;
        return event;
#else
        return result.exchange(Event::None);
#endif
    }
    State state = State::Idle;
    uint32_t since = 0;
    uint32_t pressedAt = 0;
    uint8_t activePin = HARDWARE_PIN_DISABLED;
    bool pressed = false;
    bool consumed = false;
    bool openPortal = false;
    bool previousAutoReconnect = true;
    bool havePrevious = false;

#ifdef ESP8266
    station_config previous{};
    station_config previousStored{};
    bool haveStored = false;

    static void onEvent(int event) {
        setResult(event == WPS_CB_ST_SUCCESS ? Event::Paired : Event::Error);
    }

    bool enablePairing() {
        havePrevious = wifi_station_get_config(&previous) && previous.ssid[0];
        haveStored = wifi_station_get_config_default(&previousStored);
        wifi_station_disconnect();
        wifi_wps_disable();
        return wifi_wps_enable(WPS_TYPE_PBC) && wifi_set_wps_cb(onEvent) &&
               wifi_wps_start();
    }
    void stopPairing() { wifi_wps_disable(); }
    bool connectProfile() { return wifi_station_connect(); }
    bool saveProfile() {
        station_config current{};
        return wifi_station_get_config(&current) && wifi_station_set_config(&current);
    }
    void restoreProfile() {
        wifi_station_disconnect();
        if (haveStored) wifi_station_set_config(&previousStored);
        if (havePrevious) {
            wifi_station_set_config_current(&previous);
            wifi_station_connect();
        }
    }
#else
    wifi_config_t previous{};

    static void onEvent(arduino_event_id_t event) {
        switch (event) {
        case ARDUINO_EVENT_WPS_ER_SUCCESS:
            setResult(Event::Paired);
            break;
        case ARDUINO_EVENT_WPS_ER_FAILED:
        case ARDUINO_EVENT_WPS_ER_TIMEOUT:
        case ARDUINO_EVENT_WPS_ER_PBC_OVERLAP:
            setResult(Event::Error);
            break;
        default:
            break;
        }
    }

    bool enablePairing() {
        havePrevious = esp_wifi_get_config(WIFI_IF_STA, &previous) == ESP_OK &&
                       previous.sta.ssid[0];
        esp_wifi_disconnect();
        esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);
        return esp_wifi_set_storage(WIFI_STORAGE_RAM) == ESP_OK &&
               esp_wifi_wps_enable(&config) == ESP_OK &&
               esp_wifi_wps_start(0) == ESP_OK;
    }
    void stopPairing() { esp_wifi_wps_disable(); }
    bool connectProfile() { return esp_wifi_connect() == ESP_OK; }
    bool saveProfile() {
        wifi_config_t current{};
        if (esp_wifi_get_config(WIFI_IF_STA, &current) != ESP_OK ||
            esp_wifi_set_storage(WIFI_STORAGE_FLASH) != ESP_OK) return false;
        const bool saved = esp_wifi_set_config(WIFI_IF_STA, &current) == ESP_OK;
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        return saved;
    }
    void restoreProfile() {
        esp_wifi_disconnect();
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        if (havePrevious) {
            esp_wifi_set_config(WIFI_IF_STA, &previous);
            esp_wifi_connect();
        }
    }
#endif

    void start(uint32_t now) {
        setResult(Event::None);
        previousAutoReconnect = WiFi.getAutoReconnect();
        WiFi.setAutoReconnect(false);
        if (!WiFi.enableSTA(true) || !enablePairing()) {
            stopPairing();
            fail(now);
            return;
        }
        state = State::Pairing;
        since = now;
    }

    void fail(uint32_t now) {
        restoreProfile();
        WiFi.setAutoReconnect(previousAutoReconnect);
        openPortal = true;
        state = State::Failed;
        since = now;
    }
};

#ifdef ESP8266
volatile WpsButton::Event WpsButton::result = WpsButton::Event::None;
#else
std::atomic<WpsButton::Event> WpsButton::result{WpsButton::Event::None};
#endif
