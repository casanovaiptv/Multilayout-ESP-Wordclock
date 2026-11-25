#pragma once
#include <stdint.h>

namespace ledPower {
constexpr uint8_t Full = 255;
constexpr uint8_t ChannelMa = 20;
constexpr uint8_t IdleMa = 1;

inline uint32_t estimate(uint16_t pixels, uint32_t channelSum) {
    return uint32_t(pixels) * IdleMa + (channelSum * ChannelMa + 254) / 255;
}

inline uint8_t target(uint16_t pixels, uint32_t channelSum, uint16_t budgetMa) {
    const uint32_t idle = uint32_t(pixels) * IdleMa;
    if (!channelSum || estimate(pixels, channelSum) <= budgetMa) return Full;
    if (budgetMa <= idle) return 0;
    return (uint32_t(budgetMa) - idle) * 255 * 255 / (channelSum * ChannelMa);
}

inline uint8_t scale(uint8_t channel, uint8_t gain) {
    return uint16_t(channel) * gain / Full;
}
}
