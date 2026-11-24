// BleGamepadConfiguration setters -> BleGamepad::begin(): report-size
// computation and the generic/SInput/XInput HID report descriptor builders
// writing the fixed 512-byte descriptor buffer.
#include "fuzz_config.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider fdp(data, size);
    BleGamepadConfiguration cfg;
    ConsumeConfiguration(fdp, cfg);
    GamepadSession s(cfg);
    g_sink = Fold(s.pad->getHidReportDescriptor(), s.pad->getHidReportDescriptorSize());
    g_sink = s.pad->getHidReportSize();
    return 0;
}
