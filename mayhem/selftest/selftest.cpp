// Known-answer tests for ESP32-BLE-Gamepad on the host NimBLE stand-in.
// Prints one "ok <name>" / "not ok <name>" line per check; mayhem/test.sh counts them.
#include <cstdio>
#include <cstring>
#include <vector>
#include "BleGamepad.h"

// Golden values for the default configuration (16 buttons, 8 axes, 1 hat).
#define DEFAULT_DESC_SIZE 96
#define DEFAULT_REPORT_SIZE 19
static int g_pass = 0, g_fail = 0;
static void check(bool cond, const char *name)
{
    printf("%s %s\n", cond ? "ok" : "not ok", name);
    (cond ? g_pass : g_fail)++;
}

static bool bytesEq(const uint8_t *a, std::vector<uint8_t> b)
{
    return a && memcmp(a, b.data(), b.size()) == 0;
}

static void write(NimBLECharacteristic *c, std::vector<uint8_t> v) { stub_host_write(c, v.data(), v.size()); }

static void testOutputFeature()
{
    BleGamepadConfiguration cfg;
    cfg.setAutoReport(false);
    cfg.setButtonCount(16);
    cfg.setHatSwitchCount(1);
    cfg.setEnableOutputReport(true);
    cfg.setOutputReportLength(4);
    cfg.setEnableFeatureReport(true);
    cfg.setFeatureReportLength(4);
    BleGamepad *pad = new BleGamepad();
    pad->begin(&cfg);
    NimBLECharacteristic *out = stub_find(STUB_OUTPUT);
    NimBLECharacteristic *feat = stub_find(STUB_FEATURE);
    check(out && feat, "output_feature_characteristics_created");
    check(!pad->isOutputReceived(), "output_flag_initially_clear");
    write(out, {1, 2, 3, 4});
    check(pad->isOutputReceived(), "output_flag_set_after_write");
    check(!pad->isOutputReceived(), "output_flag_cleared_after_read");
    check(bytesEq(pad->getOutputBuffer(), {1, 2, 3, 4}), "output_plain_write");
    write(out, {3, 5, 6, 7, 8});
    check(bytesEq(pad->getOutputBuffer(), {5, 6, 7, 8}), "output_report_id_stripped");
    write(out, {9, 9});
    check(bytesEq(pad->getOutputBuffer(), {9, 9, 7, 8}), "output_short_write_partial");

    // default capability report: 16 buttons, X/Y/Z/RZ/RX/RY/slider1/slider2 axes, 1 hat
    NimBLEAttValue v = stub_host_read(feat);
    check(v.size() == 4 && bytesEq(v.data(), {0xF0, 16, 8, 1}), "feature_default_caps");
    write(feat, {0xA1, 0xA2, 0xA3, 0xA4});
    check(pad->isFeatureReceived() && bytesEq(pad->getFeatureBuffer(), {0xA1, 0xA2, 0xA3, 0xA4}), "feature_plain_write");
    write(feat, {3, 0xB1, 0xB2, 0xB3, 0xB4});
    check(bytesEq(pad->getFeatureBuffer(), {0xB1, 0xB2, 0xB3, 0xB4}), "feature_report_id_stripped");
    write(feat, {1, 2, 3, 4, 5, 6});
    check(bytesEq(pad->getFeatureBuffer(), {0xB1, 0xB2, 0xB3, 0xB4}), "feature_oversize_ignored");
    uint8_t fb[2] = {0x11, 0x22};
    pad->setFeatureBuffer(fb, 2);
    v = stub_host_read(feat);
    check(v.size() == 4 && bytesEq(v.data(), {0x11, 0x22, 0xB3, 0xB4}), "feature_setFeatureBuffer_served_on_read");
    pad->end();
    delete pad;
    stub_reset();
}

static void testDescriptor()
{
    BleGamepadConfiguration cfg;
    BleGamepad *pad = new BleGamepad();
    pad->begin(&cfg);
    const uint8_t *d = pad->getHidReportDescriptor();
    printf("# default descriptor size %d, report size %d\n", pad->getHidReportDescriptorSize(), pad->getHidReportSize());
    check(pad->getHidReportDescriptorSize() == DEFAULT_DESC_SIZE, "descriptor_default_size");
    check(pad->getHidReportSize() == DEFAULT_REPORT_SIZE, "report_default_size");
    check(bytesEq(d, {0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x03}), "descriptor_default_header");
    check(d[pad->getHidReportDescriptorSize() - 1] == 0xC0, "descriptor_ends_with_end_collection");
    pad->end();
    delete pad;
    stub_reset();

    BleGamepadConfiguration sc;
    sc.setGamepadMode(GamepadMode::SInput);
    pad = new BleGamepad();
    pad->begin(&sc);
    check(pad->getHidReportDescriptorSize() == 139, "descriptor_sinput_size");
    pad->end();
    delete pad;
    stub_reset();
}

static void testGenericReport()
{
    BleGamepadConfiguration cfg;
    cfg.setAutoReport(false);
    // 128 buttons: sendGenericReport() memcpy()s all 16 button bytes into the
    // report, so keep the report at least that large (smaller reports overflow).
    cfg.setButtonCount(128);
    cfg.setHatSwitchCount(0);
    cfg.setWhichSpecialButtons(false, false, false, false, false, false, false, false);
    cfg.setWhichAxes(true, true, false, false, false, false, false, false);
    BleGamepad *pad = new BleGamepad();
    pad->begin(&cfg);
    NimBLECharacteristic *in = nullptr;
    for (NimBLECharacteristic *c : stub_characteristics())
        if (c->reportType == STUB_INPUT) in = c;
    pad->press(BUTTON_1);
    pad->press(BUTTON_8);
    check(pad->isPressed(BUTTON_8) && !pad->isPressed(BUTTON_2), "buttons_pressed_state");
    pad->setX(0x1234);
    pad->setY(-2);
    pad->sendReport();
    check(in && in->notifyCount == 0, "no_report_while_disconnected");
    stub_host_connect(true);
    pad->sendReport();
    NimBLEAttValue v = in ? in->getValue() : NimBLEAttValue();
    check(in && in->notifyCount == 1 && v.size() == 20 &&
              bytesEq(v.data(), {0x81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x34, 0x12, 0xFE, 0xFF}),
          "generic_report_packing");
    pad->release(BUTTON_1);
    pad->sendReport();
    v = in->getValue();
    check(v.size() == 20 && v.data()[0] == 0x80, "generic_report_after_release");
    pad->end();
    delete pad;
    stub_reset();
}

static void testSInput()
{
    BleGamepadConfiguration cfg;
    cfg.setGamepadMode(GamepadMode::SInput);
    cfg.setEnableRumble(true);
    BleGamepad *pad = new BleGamepad();
    pad->begin(&cfg);
    NimBLECharacteristic *cmd = stub_find_writable();
    check(cmd != nullptr, "sinput_command_characteristic");
    write(cmd, {SINPUT_COMMAND_PLAYERLED, 3});
    check(pad->isPlayerLedReceived() && pad->getPlayerLedIndex() == 3, "sinput_player_led");
    write(cmd, {SINPUT_COMMAND_HAPTIC, 0x02, 0x40, 0, 0x80, 0});
    check(pad->isRumbleReceived() && pad->getRumbleLeftAmplitude() == 0x40 && pad->getRumbleRightAmplitude() == 0x80,
          "sinput_haptic_type2");
    std::vector<uint8_t> h1(18, 0);
    h1[0] = SINPUT_COMMAND_HAPTIC; h1[1] = 0x01; h1[4] = 0x10; h1[8] = 0x20; h1[12] = 0x00; h1[13] = 0x12; h1[16] = 0x05;
    write(cmd, h1);
    check(pad->isRumbleReceived() && pad->getRumbleLeftAmplitude() == 0x20 && pad->getRumbleRightAmplitude() == 0x12,
          "sinput_haptic_type1");
    write(cmd, {SINPUT_COMMAND_JOYSTICKRGB, 0xE4, 0x1E, 0xC0});
    check(pad->isRgbReceived() && pad->getRgbRed() == 0xE4 && pad->getRgbGreen() == 0x1E && pad->getRgbBlue() == 0xC0,
          "sinput_rgb");
    NimBLECharacteristic *resp = nullptr;
    size_t before = stub_notified_bytes();
    write(cmd, {SINPUT_COMMAND_FEATURES});
    check(stub_notified_bytes() - before == SINPUT_REPORT_LEN_INPUT, "sinput_features_response_notified");
    for (NimBLECharacteristic *c : stub_characteristics())
        if (c->notifyCount == 1 && c->getValue().size() == SINPUT_REPORT_LEN_INPUT) resp = c;
    NimBLEAttValue v = resp ? resp->getValue() : NimBLEAttValue();
    check(resp && v.data()[0] == SINPUT_COMMAND_FEATURES && (v.data()[SINPUT_FEAT_IDX_CAPS0] & SINPUT_FEAT_CAP_RUMBLE),
          "sinput_features_response_content");
    pad->end();
    delete pad;
    stub_reset();
}

static void testXInput()
{
    BleGamepadConfiguration cfg;
    cfg.setGamepadMode(GamepadMode::XInputSeriesX);
    BleGamepad *pad = new BleGamepad();
    pad->begin(&cfg);
    NimBLECharacteristic *out = stub_find(STUB_OUTPUT);
    check(out != nullptr, "xinput_output_characteristic");
    write(out, {0x0F, 10, 20, 30, 40, 0, 0, 0});
    check(pad->isXInputRumbleReceived() && pad->getXInputLeftTriggerMagnitude() == 10 &&
              pad->getXInputRightTriggerMagnitude() == 20 && pad->getXInputWeakMotor() == 30 &&
              pad->getXInputStrongMotor() == 40,
          "xinput_rumble");
    write(out, {0x03, 0x0F, 1, 2, 3, 4, 0, 0, 0});
    check(pad->isXInputRumbleReceived() && pad->getXInputStrongMotor() == 4, "xinput_report_id_stripped");
    write(out, {0x0F, 99, 99});
    check(!pad->isXInputRumbleReceived() && pad->getXInputStrongMotor() == 4, "xinput_short_write_ignored");
    pad->end();
    delete pad;
    stub_reset();
}

int main()
{
    testOutputFeature();
    testDescriptor();
    testGenericReport();
    testSInput();
    testXInput();
    printf("# passed %d failed %d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
