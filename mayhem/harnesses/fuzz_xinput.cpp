// Host rumble writes to the XInput output report (BleXInputReceiver::onWrite:
// length check, Report ID stripping, XInputOutputReport struct cast).
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider fdp(data, size);
    BleGamepadConfiguration cfg;
    cfg.setAutoReport(false);
    cfg.setGamepadMode(fdp.ConsumeBool() ? GamepadMode::XInputSeriesX : GamepadMode::XInputOneS);

    GamepadSession s(cfg);
    BleGamepad *pad = s.pad;
    NimBLECharacteristic *out = stub_find(STUB_OUTPUT);
    if (!out) abort(); // XInput mode always installs the rumble receiver

    while (fdp.remaining_bytes() > 0)
    {
        std::vector<uint8_t> w = ConsumeWrite(fdp, 32);
        stub_host_write(out, w.data(), w.size());
        if (pad->isXInputRumbleReceived())
            g_sink = pad->getXInputStrongMotor() ^ pad->getXInputWeakMotor() ^
                     pad->getXInputLeftTriggerMagnitude() ^ pad->getXInputRightTriggerMagnitude();
    }
    return 0;
}
