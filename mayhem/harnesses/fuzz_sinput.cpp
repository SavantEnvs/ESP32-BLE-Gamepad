// Host commands to the SInput output report (BleSInputReceiver::onWrite:
// features request -> sendFeaturesResponse, player LED, haptics type 1/2, RGB).
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider fdp(data, size);
    BleGamepadConfiguration cfg;
    cfg.setAutoReport(false);
    cfg.setGamepadMode(GamepadMode::SInput);
    cfg.setEnableRumble(fdp.ConsumeBool());
    cfg.setEnablePlayerLED(fdp.ConsumeBool());
    cfg.setEnableSInputIMU(fdp.ConsumeBool());
    cfg.setEnableSInputRGB(fdp.ConsumeBool());
    cfg.setEnableTouchpad(fdp.ConsumeBool());
    cfg.setTouchpadCount(fdp.ConsumeIntegral<uint8_t>());
    cfg.setTouchpadFingerCount(fdp.ConsumeIntegral<uint8_t>());
    cfg.setSInputGamepadType(fdp.ConsumeIntegral<uint8_t>());
    cfg.setSInputFaceStyle(fdp.ConsumeIntegral<uint8_t>());
    cfg.setWhichAxes(fdp.ConsumeBool(), fdp.ConsumeBool(), fdp.ConsumeBool(), fdp.ConsumeBool(),
                     fdp.ConsumeBool(), fdp.ConsumeBool(), false, false);

    GamepadSession s(cfg);
    BleGamepad *pad = s.pad;
    NimBLECharacteristic *cmd = stub_find_writable();
    if (!cmd) abort(); // SInput mode always installs the command receiver

    while (fdp.remaining_bytes() > 0)
    {
        std::vector<uint8_t> w = ConsumeWrite(fdp, 64);
        stub_host_write(cmd, w.data(), w.size());
        if (pad->isPlayerLedReceived()) g_sink = pad->getPlayerLedIndex();
        if (pad->isRumbleReceived()) g_sink = pad->getRumbleLeftAmplitude() ^ pad->getRumbleRightAmplitude();
        if (pad->isRgbReceived()) g_sink = pad->getRgbRed() ^ pad->getRgbGreen() ^ pad->getRgbBlue();
    }
    return 0;
}
