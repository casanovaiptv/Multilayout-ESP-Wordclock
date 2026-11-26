#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

struct HsbColor {
    float H = 0;
    float S = 0;
    float B = 0;
};

#define DEFAULT_BRIGHTNESS 0.5f
#define ESP8266
#include "WordClockState.h"

#include "ClockTypeForTests.h"
#include "EEPROMAnything.h"
#include "Frame.h"
#include "LedGeometry.h"
#include "LedPower.h"

uint32_t clockMs = 0;
bool pressed = false;
bool reconnect = true;
int wifiStatus = 0;
bool stationAvailable = true;
bool wpsAvailable = true;
bool connecting = false;
bool saved = false;
const int WL_CONNECTED = 3;

uint32_t millis() { return clockMs; }
int digitalRead(uint8_t) { return pressed ? LOW : 1; }
void pinMode(uint8_t, int) {}

struct FakeWiFi {
    bool enableSTA(bool) { return stationAvailable; }
    bool getAutoReconnect() { return reconnect; }
    void setAutoReconnect(bool enabled) { reconnect = enabled; }
    int status() { return wifiStatus; }
} WiFi;

struct FakeWiFiManager {
    bool active = false;
    bool getConfigPortalActive() { return active; }
    void stopConfigPortal() { active = false; }
} wifiManager;

#include "WpsButton.h"

wps_st_cb_t wpsCallback = nullptr;
station_config station{{'o', 'l', 'd'}};
station_config stored = station;
void disable_extra4k_at_link_time() {}
bool wifi_station_get_config(station_config *value) {
    *value = station;
    return true;
}
bool wifi_station_get_config_default(station_config *value) {
    *value = stored;
    return true;
}
bool wifi_station_set_config_current(station_config *value) {
    station = *value;
    return true;
}
bool wifi_station_set_config(station_config *value) {
    stored = *value;
    saved = true;
    return true;
}
bool wifi_station_disconnect() {
    wifiStatus = 0;
    connecting = false;
    return true;
}
bool wifi_station_connect() {
    connecting = true;
    return true;
}
bool wifi_wps_enable(WPS_TYPE_t) { return wpsAvailable; }
bool wifi_wps_disable() { return true; }
bool wifi_wps_start() { return true; }
bool wifi_set_wps_cb(wps_st_cb_t cb) {
    wpsCallback = cb;
    return true;
}

void testMigration() {
    for (uint8_t layout : {13, 18, 11}) {
        memset(EEPROM.bytes, 0xFF, sizeof(EEPROM.bytes));
        G = {};
        G.sernr = 440;
        G.clockTypeDef = layout;
        G.buildTypeDef = BuildTypeDef::DoubleRes;
        G.secondVariant = SecondVariant::FrameSectorToggle;
        G.minuteVariant = MinuteVariant::LED4x;
        G.mqtt.port = 1883;
        strcpy(G.mqtt.topic, "legacy/wordclock");
        strcpy(G.timezone, "CET-1CEST,M3.5.0,M10.5.0/3");
        eeprom::writeAnything(0, G);
        EEPROM.bytes[511] = 2;
        const uint8_t expectedLayout = layout == 13   ? 12
                                       : layout == 18 ? 17
                                                      : 11;
        GLOBAL expected = G;
        expected.clockTypeDef = expectedLayout;
        eeprom::read();
        assert(!hardware.storedValid());
        assert(eeprom::migrateHardware());
        assert(hardware.has(hardwareSettings::RingEnabled) == (layout != 11));
        assert(hardware.ringCount == 60 && hardware.ringOffset == 8);
        assert(hardware.wpsPin == 255);
        assert(!hardware.has(hardwareSettings::RingReverse));
        assert(eeprom::write());
        assert(memcmp(&expected, EEPROM.bytes, sizeof(G)) == 0);
        assert(EEPROM.bytes[511] == 2);
        eeprom::read();
        assert(!eeprom::migrateHardware());
        assert(hardware.storedValid());

        hardware.ringCount = 48;
        hardware.ringOffset = 5;
        assert(eeprom::write());
        eeprom::read();
        assert(!eeprom::migrateHardware());
        assert(hardware.ringCount == 48 && hardware.ringOffset == 5);
        EEPROM.bytes[hardwareSettings::StorageAddress + 6] ^= 1;
        eeprom::read();
        assert(!hardware.storedValid());
    }
}

void testLayouts() {
    ClockType *layouts[] = {
#define X(name, id, var) &var,
        CLOCK_TYPES_LIST
#undef X
    };
    for (ClockType *layout : layouts) {
        for (uint8_t resolution = 0; resolution <= 4; ++resolution) {
            G.buildTypeDef = static_cast<BuildTypeDef>(resolution);
            const uint8_t scale = getLedsPerLetter(G.buildTypeDef);
            for (uint8_t variants = 0; variants < 8; ++variants) {
                G.layoutVariant[FlipHorzVert] = variants & 1;
                G.layoutVariant[ExtraLedPerRow] = variants & 2;
                G.layoutVariant[MeanderRows] = variants & 4;
                const auto disabled = ledGeometry(*layout, 0, false);
                const auto enabled = ledGeometry(*layout, 200, false);
                assert(disabled.installed == enabled.ringStart);
                assert(enabled.installed == enabled.ringStart + 200);
                assert(enabled.transmitted == enabled.installed);
                std::vector<bool> matrix(enabled.ringStart);
                for (uint8_t row = 0; row < layout->rowsWordMatrix(); ++row) {
                    for (uint8_t col = 0; col < layout->colsWordMatrix();
                         ++col) {
                        const uint16_t index =
                            layout->getFrontMatrixIndex(row, col);
                        assert((index + 1) * scale <= enabled.ringStart);
                        assert(!matrix[index]);
                        matrix[index] = true;
                    }
                }
                for (uint8_t slot = 0; slot < 3; ++slot) {
                    if (!layout->supportsMinuteVariant(
                            static_cast<MinuteVariant>(slot + 1)))
                        continue;
                    uint16_t indices[4]{};
                    layout->getMinuteArray(indices, slot);
                    for (uint16_t index : indices)
                        assert(index < enabled.ringStart);
                }
            }
        }
    }
    G = {};
    for (ClockType *layout : {layouts[12], layouts[17]}) {
        const auto legacy = ledGeometry(*layout, 60, true);
        assert(legacy.ringStart == 114 && legacy.legacyMinutes);
        assert(legacy.installed == 174 && legacy.transmitted == 234);
    }
    G.buildTypeDef = BuildTypeDef::DoubleRes;
    assert(ledGeometry(*layouts[12], 60, true).ringStart >= 220);
}

void testRingAndPower() {
    using namespace SecondsRingMapping;
    assert(physicalIndex(200, 0, 100, true) == 0);
    assert(sectorAt(1, 0) == 0);
    assert(stepOffset(0, 0, false) == 0);
    for (uint16_t count = 1; count <= MaxCount; ++count) {
        const uint16_t offset = DefaultOffset % count;
        assert(physicalIndex(0, count, offset, false) == offset);
        assert(physicalIndex(0, count, offset, true) == offset);
        assert(stepOffset(0, count, false) == count - 1);
        assert(stepOffset(count - 1, count, true) == 0);
        for (bool reverse : {false, true}) {
            std::vector<bool> hit(count);
            for (uint16_t sector = 0; sector < count; ++sector) {
                const uint16_t index =
                    physicalIndex(sector, count, offset, reverse);
                const uint16_t expected =
                    (offset + count + (reverse ? -sector : sector)) % count;
                assert(index == expected && !hit[index]);
                hit[index] = true;
            }
        }
        assert(sectorAt(59999, count) == count - 1);
        assert(sectorAt(60000, count) == 0);
    }

    for (uint16_t pixels : {48, 60, 110, 1400}) {
        for (uint16_t budget : {0, 150, 300, 3000, 60000}) {
            for (uint16_t frame = 0; frame < 256; ++frame) {
                const uint8_t colors[] = {uint8_t(frame), uint8_t(frame / 3),
                                          uint8_t(255 - frame),
                                          uint8_t(frame / 2)};
                const uint32_t sum = uint32_t(pixels) * (colors[0] + colors[1] +
                                                         colors[2] + colors[3]);
                const uint8_t gain = ledPower::target(pixels, sum, budget);
                uint32_t scaled = 0;
                for (uint8_t value : colors)
                    scaled += ledPower::scale(value, gain);
                assert(ledPower::estimate(pixels, scaled * pixels) <=
                       std::max(pixels, budget));
            }
        }
    }
}

void testAnimations() {
    SecondsFrame frame;
    hardware.flags = hardwareSettings::RingEnabled;
    for (uint16_t count : {1, 48, 60, 120, 200}) {
        hardware.ringCount = count;
        for (SecondVariant animation :
             {SecondVariant::FrameDot, SecondVariant::FrameSector,
              SecondVariant::FrameSectorToggle}) {
            G.secondVariant = animation;
            for (uint8_t minute : {0, 1}) {
                _minute = minute;
                for (uint32_t time : {0, 12345, 30000, 59999, 60000}) {
                    G.prog = COMMAND_IDLE;
                    parametersChanged = false;
                    frame.reset();
                    frame.loop(time);
                    assert(G.prog == COMMAND_MODE_WORD_CLOCK &&
                           parametersChanged);
                    const uint16_t sector =
                        SecondsRingMapping::sectorAt(time, count);
                    for (uint16_t i = 0; i < count; ++i) {
                        const bool expected =
                            animation == SecondVariant::FrameDot
                                ? i == sector
                                : (i <= sector) !=
                                      (animation ==
                                           SecondVariant::FrameSectorToggle &&
                                       minute);
                        assert(frameArray[i] == expected);
                    }
                }
            }
        }
    }
    hardware.flags = 0;
    G.prog = COMMAND_IDLE;
    parametersChanged = false;
    frame.loop(3000);
    assert(G.prog == COMMAND_IDLE && !parametersChanged);
}

void testWps() {
    hardware.flags = hardwareSettings::WpsEnabled;
    hardware.wpsPin = 12;
    G.hardwarePins = {3, 255, 255, 255};
    G.i2cSdaPin = 4;
    G.i2cSclPin = 5;
    assert(!wpsPinAllowed(4) && !wpsPinAllowed(3) && !wpsPinAllowed(6));
    assert(wpsPinAllowed(12));
    using State = WpsButton::State;

    for (int scenario = 0; scenario < 5; ++scenario) {
        WpsButton button;
        button.begin();
        saved = false;
        wifiManager.active = true;
        wpsAvailable = scenario != 4;
        pressed = true;
        assert(button.update() == State::Idle);
        clockMs += 2999;
        assert(button.update() == State::Idle);
        ++clockMs;
        assert(button.update() ==
               (scenario == 4 ? State::Failed : State::Pairing));
        assert(!wifiManager.active);
        if (scenario == 0 || scenario == 3) {
            station = {{'n', 'e', 'w'}};
            wpsCallback(WPS_CB_ST_SUCCESS);
            assert(button.update() == State::Connecting && connecting);
            if (scenario == 0) {
                wifiStatus = WL_CONNECTED;
                assert(button.update() == State::Success);
                assert(saved && stored.ssid[0] == 'n');
                assert(!button.needsPortal());
            } else {
                clockMs += 20000;
                assert(button.update() == State::Failed);
                assert(button.needsPortal() && !button.needsPortal());
            }
        } else if (scenario < 4) {
            if (scenario == 1)
                wpsCallback(WPS_CB_ST_FAILED);
            else
                clockMs += 120000;
            assert(button.update() == State::Failed);
            assert(button.needsPortal() && !button.needsPortal());
        }
        assert(reconnect);
        clockMs += 2000;
        assert(button.update() == State::Idle);
        assert(button.update() ==
               State::Idle); // Holding cannot start a second attempt.
        pressed = false;
        button.update();
    }
}

int main() {
    testMigration();
    testLayouts();
    testRingAndPower();
    testAnimations();
    testWps();
    puts("Legacy EEPROM, 35 layouts, ring bounds/directions, power frames and "
         "mock WPS passed.");
}
