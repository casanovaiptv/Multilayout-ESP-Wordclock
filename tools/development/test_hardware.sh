#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_dir="$(mktemp -d)"
trap 'rm -rf "$test_dir"' EXIT
cat > "$test_dir/Arduino.h" <<'HEADER'
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <algorithm>
#include <initializer_list>
using byte = uint8_t;
using std::min;
using std::max;
#define LOW 0
#define INPUT_PULLUP 2
void pinMode(uint8_t, int);
int digitalRead(uint8_t);
uint32_t millis();
inline void delay(uint32_t) {}
struct SerialStub {
    template <typename ...Args> void printf(const char *, Args...) {}
    template <typename T> void print(T) {}
    template <typename T> void println(T) {}
} Serial;
HEADER
printf '#define VERSION "test"\n' > "$test_dir/version.gen.h"
for layout in include/WordClockTypes/*.hpp; do
    printf '#include "%s"\n' "${layout#include/}"
done > "$test_dir/ClockTypeForTests.h"
cat > "$test_dir/EEPROM.h" <<'HEADER'
#pragma once
struct EEPROMStub {
    uint8_t bytes[512]{};
    uint8_t read(int offset) { return bytes[offset]; }
    void write(int offset, uint8_t value) { bytes[offset] = value; }
    bool commit() { return true; }
} EEPROM;
HEADER
printf 'void disable_extra4k_at_link_time();\n' > "$test_dir/coredecls.h"
cat > "$test_dir/user_interface.h" <<'HEADER'
#pragma once
typedef struct station_config { uint8_t ssid[32]; } station_config;
enum WPS_TYPE_t { WPS_TYPE_PBC = 1 };
enum wps_cb_status { WPS_CB_ST_SUCCESS, WPS_CB_ST_FAILED, WPS_CB_ST_TIMEOUT };
typedef void (*wps_st_cb_t)(int);
bool wifi_station_get_config(station_config *);
bool wifi_station_get_config_default(station_config *);
bool wifi_station_set_config(station_config *);
bool wifi_station_set_config_current(station_config *);
bool wifi_station_disconnect();
bool wifi_station_connect();
bool wifi_wps_enable(WPS_TYPE_t);
bool wifi_wps_disable();
bool wifi_wps_start();
bool wifi_set_wps_cb(wps_st_cb_t);
HEADER
"${CXX:-g++}" -std=c++11 -g -fsanitize=undefined -I "$test_dir" -I include \
    tools/development/test_hardware.cpp -o "$test_dir/test"
"$test_dir/test"
