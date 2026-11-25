#pragma once
#include <stdint.h>

namespace SecondsRingMapping {
constexpr uint16_t DefaultCount = 60;
constexpr uint16_t MaxCount = 200;
constexpr uint16_t DefaultOffset = 8;

inline uint16_t stepOffset(uint16_t offset, uint16_t count, bool forward) {
    if (!count) return 0;
    return (uint32_t(offset) + (forward ? 1 : count - 1)) % count;
}

// Map a logical sector to the physical ring, independent of front layout.
inline uint16_t physicalIndex(uint16_t sector, uint16_t count,
                              uint16_t offset, bool reverse) {
    if (!count) return 0;
    const uint32_t position = sector % count;
    return (uint32_t(offset % count) + (reverse ? count - position : position)) % count;
}

// Map elapsed milliseconds within a minute to a logical sector.
inline uint16_t sectorAt(uint32_t elapsedMs, uint16_t count) {
    return uint32_t(elapsedMs % 60000UL) * count / 60000UL;
}
}
