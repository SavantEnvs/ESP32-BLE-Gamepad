// The sketch-facing input-report API after begin(): buttons, special buttons,
// hats, axes, simulation/motion/touchpad controls, battery/power state, and the
// per-mode report packing in sendReport() (generic, SInput, XInput).
#include "fuzz_config.h"

static uint8_t Button(FuzzedDataProvider &fdp)
{
    // Buttons are BUTTON_1..BUTTON_128 (BleGamepadConfiguration.h).
    return fdp.ConsumeIntegralInRange<uint8_t>(1, 128);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider fdp(data, size);
    BleGamepadConfiguration cfg;
    ConsumeConfiguration(fdp, cfg);
    GamepadSession s(cfg);
    BleGamepad *pad = s.pad;
    stub_host_connect(fdp.ConsumeBool());

    while (fdp.remaining_bytes() > 0)
    {
        int16_t a = fdp.ConsumeIntegral<int16_t>();
        int16_t b = fdp.ConsumeIntegral<int16_t>();
        switch (fdp.ConsumeIntegralInRange<int>(0, 27))
        {
        case 0: pad->press(Button(fdp)); break;
        case 1: pad->release(Button(fdp)); break;
        case 2: g_sink = pad->isPressed(Button(fdp)); break;
        case 3: pad->setButtonsFromMask(fdp.ConsumeIntegral<uint64_t>()); break;
        case 4: {
            uint8_t bits[16] = {0};
            std::vector<uint8_t> v = fdp.ConsumeBytes<uint8_t>(16);
            memcpy(bits, v.data(), v.size());
            pad->setAllButtons(bits);
            break;
        }
        case 5: pad->pressSpecialButton(fdp.ConsumeIntegralInRange<uint8_t>(0, POSSIBLESPECIALBUTTONS - 1)); break;
        case 6: pad->releaseSpecialButton(fdp.ConsumeIntegralInRange<uint8_t>(0, POSSIBLESPECIALBUTTONS - 1)); break;
        case 7: pad->setAxes(a, b, a, b, a, b, a, b); break;
        case 8: pad->setHIDAxes(a, b, b, a, a, b, b, a); break;
        case 9: pad->setHats(fdp.ConsumeIntegral<int8_t>(), fdp.ConsumeIntegral<int8_t>(),
                             fdp.ConsumeIntegral<int8_t>(), fdp.ConsumeIntegral<int8_t>()); break;
        case 10: pad->setHat(fdp.ConsumeIntegral<int8_t>()); break;
        case 11: pad->setLeftThumb(a, b); pad->setRightThumb(b, a); break;
        case 12: pad->setRightThumbAndroid(a, b); pad->setTriggers(b, a); break;
        case 13: pad->setSimulationControls(a, b, a, b, a); break;
        case 14: pad->setMotionControls(a, b, a, b, a, b); break;
        case 15: pad->setTouchpad(fdp.ConsumeIntegral<uint8_t>(), a, b, fdp.ConsumeIntegral<uint16_t>()); break;
        case 16: pad->setBatteryLevel(fdp.ConsumeIntegral<uint8_t>()); break;
        case 17: pad->setPowerStateAll(fdp.ConsumeIntegral<uint8_t>(), fdp.ConsumeIntegral<uint8_t>(),
                                       fdp.ConsumeIntegral<uint8_t>(), fdp.ConsumeIntegral<uint8_t>()); break;
        case 18: pad->sendReport(); break;
        case 19: pad->resetButtons(); break;
        case 20: pad->pressStart(); pad->pressSelect(); pad->pressMenu(); pad->pressHome(); break;
        case 21: pad->pressBack(); pad->pressVolumeInc(); pad->pressVolumeDec(); pad->pressVolumeMute(); break;
        case 22: pad->releaseStart(); pad->releaseHome(); pad->releaseBack(); pad->releaseVolumeMute(); break;
        case 23: pad->setSliders(a, b); pad->setRudder(a); pad->setThrottle(b); break;
        case 24: pad->setAccelerator(a); pad->setBrake(b); pad->setSteering(a); break;
        case 25: pad->setGyroscope(a, b, a); pad->setAccelerometer(b, a, b); break;
        case 26: if (fdp.ConsumeBool()) stub_host_disconnect(); else stub_host_connect(fdp.ConsumeBool()); break;
        case 27: pad->setX(a); pad->setY(b); pad->setZ(a); pad->setRZ(b); pad->setRX(a); pad->setRY(b); break;
        }
    }
    pad->sendReport();
    return 0;
}
