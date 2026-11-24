// Shared helpers for the ESP32-BLE-Gamepad harnesses. The library runs on top of
// the host NimBLE stand-in in mayhem/host/; every harness drives it through the
// public API (BleGamepadConfiguration -> BleGamepad::begin()) and delivers host
// traffic through the GATT callbacks begin() installed, as the BLE stack would.
#pragma once
#include <fuzzer/FuzzedDataProvider.h>
#include <cstdlib>
#include "BleGamepad.h"

// One gamepad per input; end() + stub_reset() free everything begin() made,
// so nothing survives into the next input.
struct GamepadSession
{
    BleGamepad *pad;
    explicit GamepadSession(BleGamepadConfiguration &cfg)
    {
        pad = new BleGamepad("Fuzz Gamepad", "Fuzz", 50);
        pad->begin(&cfg);
        // The descriptor is assembled in a fixed 512-byte member array
        // (BleGamepad.h tempHidReportDescriptor); a size past it means the
        // builder already wrote out of bounds.
        if (pad->getHidReportDescriptorSize() > (int)sizeof(uint8_t[512]))
            abort();
    }
    ~GamepadSession()
    {
        pad->end();
        delete pad;
        stub_reset();
    }
};

// Drain a host-written byte string of fuzzer-chosen length (0..maxLen).
static inline std::vector<uint8_t> ConsumeWrite(FuzzedDataProvider &fdp, size_t maxLen)
{
    size_t n = fdp.ConsumeIntegralInRange<size_t>(0, maxLen);
    return fdp.ConsumeBytes<uint8_t>(n);
}

static inline uint8_t Fold(const uint8_t *p, size_t n)
{
    uint8_t x = 0;
    for (size_t i = 0; i < n; i++) x ^= p[i];
    return x;
}

static volatile uint8_t g_sink;
