#pragma once

#include "LedStripInterface.h"
#include "LedPower.h"
#include <memory>
#include "NeoMultiFeature.hpp"

uint32_t estimatedLedMa = 0;
bool powerLimiting = false;
bool powerRecovering = false;

class LedPowerOutput {
    std::unique_ptr<uint8_t[]> backup;
    uint8_t gain = ledPower::Full;
    uint32_t lastRise = 0;

public:
    template <typename Strip>
    void show(Strip &strip) {
        const size_t size = strip.PixelsSize();
        const size_t physicalSize = physicalLedCount * strip.PixelSize();
        uint8_t *pixels = strip.Pixels();
        uint32_t sum = 0;
        for (size_t i = 0; i < physicalSize && i < size; ++i) {
            sum += pixels[i];
        }

        const bool enabled = hardware.has(hardwareSettings::PowerEnabled);
        const uint16_t budget = hardware.supplyMa - hardware.reserveMa;
        const uint8_t target = enabled
                                   ? ledPower::target(physicalLedCount, sum, budget)
                                   : ledPower::Full;
        const uint32_t now = millis();
        if (!enabled || target < gain) {
            gain = target;
            lastRise = now;
        } else if (gain < target && now - lastRise >= 25) {
            gain += min<uint32_t>(target - gain, (now - lastRise) / 25);
            lastRise = now;
        } else if (gain == target) {
            lastRise = now;
        }
        powerRecovering = enabled && gain < target;
        powerLimiting = enabled &&
                        (gain < ledPower::Full ||
                         ledPower::estimate(physicalLedCount, sum) > budget);
        if (!enabled) {
            backup.reset();
        }

        if (gain < ledPower::Full) {
            if (!backup) backup.reset(new (std::nothrow) uint8_t[size]);
            if (!backup) {
                // Fail with LEDs dark if an output buffer cannot be reserved.
                strip.ClearTo(0);
                strip.Show();
                estimatedLedMa = ledPower::estimate(physicalLedCount, 0);
                return;
            }
            memcpy(backup.get(), pixels, size);
            sum = 0;
            for (size_t i = 0; i < size; ++i) {
                pixels[i] = ledPower::scale(pixels[i], gain);
                if (i < physicalSize) sum += pixels[i];
            }
            strip.Dirty();
            strip.Show(false);
            // RMT uses two buffers and swaps them on Show. ESP8266 DMA
            // encodes into its transmit buffer, and bit bang is synchronous.
            memcpy(strip.Pixels(), backup.get(), size);
            strip.Dirty();
        } else {
            strip.Show();
        }
        estimatedLedMa = ledPower::estimate(physicalLedCount, sum);
    }
};

template <typename T_METHOD>
class RgbLedStripAdapter : public LedStripInterface {
private:
    NeoPixelBus<NeoMultiFeature, T_METHOD> *strip;
    LedPowerOutput output;

public:
    explicit RgbLedStripAdapter(uint16_t pixelCount) {
        strip = new NeoPixelBus<NeoMultiFeature, T_METHOD>(pixelCount);
        strip->Begin();
    }

    RgbLedStripAdapter(uint16_t pixelCount, uint8_t pin) {
        strip = new NeoPixelBus<NeoMultiFeature, T_METHOD>(pixelCount, pin);
        strip->Begin();
    }

    ~RgbLedStripAdapter() override { delete strip; }

    void setRgbPixel(uint16_t index, HsbColor color) override {
        strip->SetPixelColor(index, color);
    }

    void setRgbwPixel(uint16_t index, RgbwColor color) override {
        strip->SetPixelColor(index, RgbColor(color.R, color.G, color.B));
    }

    RgbColor getPixel(uint16_t index) override {
        return strip->GetPixelColor(index);
    }

    void clearPixel(uint16_t index) override { strip->SetPixelColor(index, 0); }

    void clearAll() override { strip->ClearTo(0); }

    void show() override { output.show(*strip); }
};

template <typename T_METHOD>
class RgbwLedStripAdapter : public LedStripInterface {
private:
    NeoPixelBus<NeoGrbwFeature, T_METHOD> *strip;
    LedPowerOutput output;

public:
    explicit RgbwLedStripAdapter(uint16_t pixelCount) {
        strip = new NeoPixelBus<NeoGrbwFeature, T_METHOD>(pixelCount);
        strip->Begin();
    }

    RgbwLedStripAdapter(uint16_t pixelCount, uint8_t pin) {
        strip = new NeoPixelBus<NeoGrbwFeature, T_METHOD>(pixelCount, pin);
        strip->Begin();
    }

    ~RgbwLedStripAdapter() override { delete strip; }

    void setRgbPixel(uint16_t index, HsbColor color) override {
        RgbColor rgbColor(color);
        strip->SetPixelColor(index,
                             RgbwColor(rgbColor.R, rgbColor.G, rgbColor.B, 0));
    }

    void setRgbwPixel(uint16_t index, RgbwColor color) override {
        strip->SetPixelColor(index, color);
    }

    RgbColor getPixel(uint16_t index) override {
        return RgbColor(strip->GetPixelColor(index));
    }

    void clearPixel(uint16_t index) override { strip->SetPixelColor(index, 0); }

    void clearAll() override { strip->ClearTo(0); }

    void show() override { output.show(*strip); }
};
