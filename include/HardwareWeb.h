#pragma once

namespace hardwareWeb {
constexpr uint8_t NoClient = UINT8_MAX;
uint8_t calibrationOwner = NoClient;
uint16_t calibrationOffset = 0;
uint32_t calibrationTouched = 0;

bool calibrating() { return calibrationOwner != NoClient; }

void restoreDisplay() {
    activeLedStrip->clearAll();
    G.progInit = true;
    if (G.prog == COMMAND_IDLE)
        G.prog = COMMAND_MODE_WORD_CLOCK;
    parametersChanged = true;
    secondsFrame.reset();
}

void disconnected(uint8_t client) {
    if (calibrationOwner == client && calibrating()) {
        calibrationOwner = NoClient;
        restoreDisplay();
    }
}

void preview() {
    activeLedStrip->clearAll();
    led.setPixel(ringStart + calibrationOffset, HsbColor(0.48f, 1.f, 0.25f));
    led.show();
}

bool persist(const hardwareSettings::Settings &settings) {
    const auto previous = hardware;
    hardware = settings;
    if (eeprom::write())
        return true;
    hardware = previous;
    eeprom::writeAnything(hardwareSettings::StorageAddress, previous.sealed());
    return false;
}

void reply(uint8_t client, const char *error, bool telemetry = false) {
    StaticJsonDocument<512> output;
    output["command"] = "hardware";
    output["error"] = error;
    output["telemetry"] = telemetry;
    output["enabled"] = ringCount() != 0;
    output["count"] = hardware.ringCount;
    output["offset"] = hardware.ringOffset;
    output["animation"] = static_cast<uint8_t>(G.secondVariant);
    output["reverse"] = hardware.has(hardwareSettings::RingReverse);
    output["limit"] = hardware.has(hardwareSettings::PowerEnabled);
    output["supply"] = hardware.supplyMa;
    output["reserve"] = hardware.reserveMa;
    output["ledMa"] = estimatedLedMa;
    output["limiting"] = powerLimiting;
    output["wps"] = hardware.has(hardwareSettings::WpsEnabled) &&
                    wpsPinAllowed(hardware.wpsPin);
    output["pin"] = hardware.wpsPin;
    output["calibrating"] = calibrationOwner == client;
    output["preview"] = calibrationOffset;
    char buffer[384];
    const size_t length = serializeJson(output, buffer, sizeof(buffer));
    webSocket.sendTXT(client, buffer, length);
}

void handle(uint8_t client, const uint8_t *payload, size_t length) {
    StaticJsonDocument<448> command;
    if (length > 256 || deserializeJson(command, payload, length)) {
        reply(client, "invalid");
        return;
    }

    using namespace hardwareSettings;
    const char *action = command["hardware"] | "";
    auto settings = hardware;
    const char *error = nullptr;
    if (!strcmp(action, "status")) {
        reply(client, nullptr, true);
        return;
    }

    if (calibrating() && calibrationOwner != client) {
        reply(client, "busy");
        return;
    }

    if (!strcmp(action, "apply")) {
        if (calibrating()) {
            reply(client, "busy");
            return;
        }
        if (!command["enabled"].is<bool>() ||
            !command["count"].is<uint16_t>() ||
            !command["offset"].is<uint16_t>() ||
            !command["reverse"].is<bool>() || !command["limit"].is<bool>() ||
            !command["supply"].is<uint16_t>() ||
            !command["reserve"].is<uint16_t>() || !command["wps"].is<bool>() ||
            !command["pin"].is<uint8_t>()) {
            reply(client, "invalid");
            return;
        }

        settings.flags &= LegacyWiring;
        if (command["enabled"].as<bool>())
            settings.flags |= RingEnabled;
        if (command["reverse"].as<bool>())
            settings.flags |= RingReverse;
        if (command["limit"].as<bool>())
            settings.flags |= PowerEnabled;
        if (command["wps"].as<bool>())
            settings.flags |= WpsEnabled;
        settings.ringCount = command["count"];
        settings.ringOffset = command["offset"];
        settings.supplyMa = command["supply"];
        settings.reserveMa = command["reserve"];
        settings.wpsPin = command["pin"];
        if (!settings.valid()) {
            error = "invalid";
        } else if (settings.has(WpsEnabled) &&
                   !wpsPinAllowed(settings.wpsPin)) {
            error = "gpio";
        } else {
            const auto previousMode = G.secondVariant;
            if (settings.has(RingEnabled) && !ringCount() &&
                G.secondVariant == SecondVariant::Off) {
                G.secondVariant = SecondVariant::FrameDot;
            }
            if (persist(settings)) {
                clockWork.initLedStrip(G.Colortype);
                restoreDisplay();
            } else {
                G.secondVariant = previousMode;
                eeprom::writeAnything(0, G);
                error = "storage";
            }
        }
    } else if (!strcmp(action, "start") && ringCount()) {
        calibrationOwner = client;
        calibrationTouched = millis();
        calibrationOffset = hardware.ringOffset;
        preview();
    } else if ((!strcmp(action, "minus") || !strcmp(action, "plus")) &&
               calibrating() && ringCount()) {
        calibrationOffset = SecondsRingMapping::stepOffset(
            calibrationOffset, ringCount(), !strcmp(action, "plus"));
        calibrationTouched = millis();
        preview();
    } else if (!strcmp(action, "save") && calibrating() && ringCount()) {
        settings.ringOffset = calibrationOffset;
        if (persist(settings)) {
            disconnected(client);
        } else {
            error = "storage";
        }
    } else if (!strcmp(action, "cancel") && calibrating()) {
        disconnected(client);
    } else {
        error = "invalid";
    }
    reply(client, error);
}
} // namespace hardwareWeb
