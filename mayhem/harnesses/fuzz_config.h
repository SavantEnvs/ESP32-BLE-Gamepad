// Decodes fuzz bytes into BleGamepadConfiguration setter calls. Every field is
// a delta from the library's default configuration (booleans XOR, numbers add),
// so the empty input is the default gamepad and every configuration stays reachable.
#pragma once
#include "fuzz_common.h"

static inline void ConsumeConfiguration(FuzzedDataProvider &fdp, BleGamepadConfiguration &cfg)
{
#define FLIP(name) cfg.set##name(cfg.get##name() ^ fdp.ConsumeBool())
#define ADD(name, T) cfg.set##name((T)(cfg.get##name() + fdp.ConsumeIntegral<T>()))
    FLIP(AutoReport);
    cfg.setGamepadMode((GamepadMode)fdp.ConsumeIntegralInRange<uint8_t>(0, 3));
    ADD(ControllerType, uint8_t);
    ADD(HidReportId, uint8_t);
    // BleGamepad.h: "_buttons[16]; // 8 bits x 16 bytes = 128 bits --> 128 button max"
    cfg.setButtonCount((cfg.getButtonCount() + fdp.ConsumeIntegral<uint8_t>()) % 129);
    // BleGamepad keeps state for hats 1..4 only (_hat1.._hat4, setHats()).
    cfg.setHatSwitchCount((cfg.getHatSwitchCount() + fdp.ConsumeIntegral<uint8_t>()) % 5);
    FLIP(IncludeStart); FLIP(IncludeSelect); FLIP(IncludeMenu); FLIP(IncludeHome);
    FLIP(IncludeBack); FLIP(IncludeVolumeInc); FLIP(IncludeVolumeDec); FLIP(IncludeVolumeMute);
    FLIP(IncludeXAxis); FLIP(IncludeYAxis); FLIP(IncludeZAxis); FLIP(IncludeRxAxis);
    FLIP(IncludeRyAxis); FLIP(IncludeRzAxis); FLIP(IncludeSlider1); FLIP(IncludeSlider2);
    FLIP(IncludeRudder); FLIP(IncludeThrottle); FLIP(IncludeAccelerator); FLIP(IncludeBrake);
    FLIP(IncludeSteering); FLIP(IncludeGyroscope); FLIP(IncludeAccelerometer);
    ADD(Vid, uint16_t); ADD(Pid, uint16_t); ADD(GuidVersion, uint16_t);
    ADD(AxesMin, int16_t); ADD(AxesMax, int16_t);
    ADD(SimulationMin, int16_t); ADD(SimulationMax, int16_t);
    ADD(MotionMin, int16_t); ADD(MotionMax, int16_t);
    FLIP(EnableOutputReport);
    cfg.setOutputReportLength((cfg.getOutputReportLength() + fdp.ConsumeIntegral<uint16_t>()) % 513);
    FLIP(EnableFeatureReport);
    cfg.setFeatureReportLength((cfg.getFeatureReportLength() + fdp.ConsumeIntegral<uint16_t>()) % 513);
    FLIP(EnableRumble); FLIP(EnablePlayerLED); FLIP(EnableSInput); FLIP(EnableSInputIMU); FLIP(EnableSInputRGB);
    ADD(TXPowerLevel, int8_t);
    ADD(SInputGamepadType, uint8_t); ADD(SInputFaceStyle, uint8_t);
    FLIP(EnableTouchpad);
    ADD(TouchpadCount, uint8_t); ADD(TouchpadFingerCount, uint8_t);
#undef FLIP
#undef ADD
}
