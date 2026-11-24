// Host writes to the generic HID Output and Feature report characteristics
// (BleOutputReceiver::onWrite, BleFeatureReceiver::onWrite/onRead, Report ID
// stripping in BleReportUtils.h) and the sketch-side buffer API
// (getOutputBuffer/getFeatureBuffer/setFeatureBuffer) — the code b65d222b18 changed.
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider fdp(data, size);
    BleGamepadConfiguration cfg;
    cfg.setAutoReport(false);
    cfg.setHidReportId(fdp.ConsumeIntegral<uint8_t>());
    uint16_t outLen = fdp.ConsumeIntegralInRange<uint16_t>(0, 512);
    uint16_t featLen = fdp.ConsumeIntegralInRange<uint16_t>(0, 512);
    cfg.setEnableOutputReport(true);
    cfg.setOutputReportLength(outLen);
    cfg.setEnableFeatureReport(true);
    cfg.setFeatureReportLength(featLen);
    cfg.setButtonCount(fdp.ConsumeIntegralInRange<uint16_t>(0, 128));
    cfg.setHatSwitchCount(fdp.ConsumeIntegralInRange<uint8_t>(0, 4));

    GamepadSession s(cfg);
    BleGamepad *pad = s.pad;
    NimBLECharacteristic *out = stub_find(STUB_OUTPUT);
    NimBLECharacteristic *feat = stub_find(STUB_FEATURE);
    if (!out || !feat) abort(); // begin() must create both when enabled

    while (fdp.remaining_bytes() > 0)
    {
        switch (fdp.ConsumeIntegralInRange<int>(0, 5))
        {
        case 0: {
            std::vector<uint8_t> w = ConsumeWrite(fdp, outLen + 2);
            stub_host_write(out, w.data(), w.size());
            break;
        }
        case 1: {
            std::vector<uint8_t> w = ConsumeWrite(fdp, featLen + 2);
            stub_host_write(feat, w.data(), w.size());
            break;
        }
        case 2: {
            NimBLEAttValue v = stub_host_read(feat);
            g_sink = Fold(v.data(), v.size());
            break;
        }
        case 3: {
            std::vector<uint8_t> w = ConsumeWrite(fdp, featLen + 2);
            w.push_back(0); // keep data() non-null for a zero-length copy
            pad->setFeatureBuffer(w.data(), (uint16_t)(w.size() - 1));
            break;
        }
        case 4:
            if (pad->isOutputReceived())
            {
                uint8_t *b = pad->getOutputBuffer();
                if (b && outLen) g_sink = b[outLen - 1];
            }
            break;
        case 5:
            if (pad->isFeatureReceived() || fdp.ConsumeBool())
            {
                uint8_t *b = pad->getFeatureBuffer();
                if (b) g_sink = Fold(b, featLen);
            }
            break;
        }
    }
    return 0;
}
