#pragma once
#include "WordClockTypes/ClockType.hpp"

struct LedGeometry {
    uint16_t ringStart;
    uint16_t installed;
    uint16_t transmitted;
    bool legacyMinutes;
};

inline LedGeometry ledGeometry(ClockType &layout, uint16_t ringLeds,
                               bool legacyWiring) {
    uint16_t matrixEnd = layout.minimumMatrixPixels();
    for (uint8_t row = 0; row < layout.rowsWordMatrix(); ++row) {
        for (uint8_t col = 0; col < layout.colsWordMatrix(); ++col) {
            const uint16_t end = layout.getFrontMatrixIndex(row, col) + 1;
            if (end > matrixEnd) matrixEnd = end;
        }
    }
    matrixEnd *= getLedsPerLetter(G.buildTypeDef);
    if (legacyWiring && matrixEnd <= 110) {
        const uint16_t installed = 114 + ringLeds;
        // Also send black to the end of a formerly 120-pixel legacy ring.
        return {114, installed, installed < 234 ? uint16_t(234) : installed, true};
    }

    uint16_t frontEnd = matrixEnd;
    const MinuteVariant modes[] = {MinuteVariant::LED4x, MinuteVariant::LED7x,
                                   MinuteVariant::Corners};
    for (uint8_t mode = 0; mode < 3; ++mode) {
        if (!layout.supportsMinuteVariant(modes[mode]) &&
            !(mode == 0 && layout.hasMinuteInWords())) continue;
        uint16_t indices[4]{};
        layout.getMinuteArray(indices, mode);
        for (uint16_t index : indices) {
            const uint16_t end = index + 1;
            if (end > frontEnd) frontEnd = end;
        }
    }
    const uint16_t installed = frontEnd + ringLeds;
    return {frontEnd, installed, installed, false};
}
