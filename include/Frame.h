#pragma once

#include "WordClockState.h"

class SecondsFrame {
private:
    uint16_t lastSector = UINT16_MAX;
    uint8_t lastMinute = UINT8_MAX;

public:
    void reset() { lastSector = UINT16_MAX; }

    void loop(uint32_t elapsed) {
        if (!ringCount() || G.secondVariant == SecondVariant::Off ||
            (G.prog != COMMAND_IDLE && G.prog != COMMAND_MODE_WORD_CLOCK)) {
            reset();
            return;
        }

        const uint16_t sector =
            SecondsRingMapping::sectorAt(elapsed, ringCount());
        if (sector == lastSector && lastMinute == _minute &&
            !parametersChanged) {
            return;
        }
        lastSector = sector;
        lastMinute = _minute;
        for (uint16_t i = 0; i < ringCount(); ++i) {
            switch (G.secondVariant) {
            case SecondVariant::FrameDot:
                frameArray[i] = i == sector;
                break;
            case SecondVariant::FrameSector:
                frameArray[i] = i <= sector;
                break;
            case SecondVariant::FrameSectorToggle:
                frameArray[i] = (i <= sector) != (_minute % 2 != 0);
                break;
            default:
                frameArray[i] = false;
                break;
            }
        }
        if (G.conf == COMMAND_IDLE) {
            parametersChanged = true;
            G.prog = COMMAND_MODE_WORD_CLOCK;
        }
    }
};
