#pragma once

#include "SecondsRingMapping.h"
#include <stdint.h>

namespace hardwareSettings {
constexpr uint16_t StorageAddress = 480;
constexpr uint32_t Signature = 0x57434801;
constexpr uint8_t RingEnabled = 1;
constexpr uint8_t RingReverse = 2;
constexpr uint8_t LegacyWiring = 4;
constexpr uint8_t PowerEnabled = 8;
constexpr uint8_t WpsEnabled = 16;
constexpr uint16_t MaxPowerMa = 60000;

struct Settings {
    uint32_t signature = Signature;
    uint16_t ringCount = SecondsRingMapping::DefaultCount;
    uint16_t ringOffset = SecondsRingMapping::DefaultOffset;
    uint16_t supplyMa = 2000;
    uint16_t reserveMa = 300;
    uint8_t flags = 0;
    uint8_t wpsPin = 19;
    uint16_t checksum = 0;

    bool has(uint8_t flag) const { return (flags & flag) != 0; }

    bool valid() const {
        return signature == Signature && ringCount > 0 &&
               ringCount <= SecondsRingMapping::MaxCount &&
               ringOffset < ringCount && supplyMa > 0 &&
               supplyMa <= MaxPowerMa && reserveMa <= supplyMa &&
               (flags & ~31) == 0;
    }

    Settings sealed() const {
        Settings record = *this;
        uint16_t crc = 0xFFFF;
        const uint16_t values[] = {uint16_t(signature >> 16),
                                   uint16_t(signature),
                                   ringCount,
                                   ringOffset,
                                   supplyMa,
                                   reserveMa,
                                   uint16_t((flags << 8) | wpsPin)};
        for (uint16_t value : values) {
            crc ^= value;
            for (uint8_t bit = 0; bit < 16; ++bit) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
            }
        }
        record.checksum = crc;
        return record;
    }

    bool storedValid() const {
        return valid() && checksum == sealed().checksum;
    }
};

static_assert(sizeof(Settings) == 16, "EEPROM extension size changed");

inline Settings migrate(uint8_t layout) {
    Settings result;
    if (layout == 13 || layout == 18) {
        result.flags = RingEnabled | LegacyWiring;
    }
    return result;
}

inline uint8_t canonicalLayout(uint8_t layout) {
    if (layout == 13)
        return 12;
    if (layout == 18)
        return 17;
    return layout;
}
} // namespace hardwareSettings
