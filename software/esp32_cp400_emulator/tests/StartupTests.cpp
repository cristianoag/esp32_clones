#include <assert.h>
#include <stdint.h>
#include <string.h>

#define CONFIG_IDF_TARGET_ESP32S3 1
#include "BlinkConfig.inc"
static_assert(BLINK_GPIO == -1, "The default activity pin must not drive BOOT GPIO0.");
constexpr unsigned HID_BOOT_REPORT_SIZE = 8;
static uint8_t report[8];
static unsigned menuKey = 69;
void UpdateKeyMap(uint8_t *released) {
    memcpy(report, released, sizeof(report));
    menuKey = released[2];
}
#include "ClearKeyboard.inc"

bool keysReady, nativeStarted, softStarted;
void fillKeysStruct() { keysReady = true; }
void CP400UsbKeyboard_Start() { assert(keysReady && !softStarted); nativeStarted = true; }
void Default_USB_ConfigDescCB() {}
void Default_USB_IfaceDescCb() {}
void Default_USB_HIDDevDescCb() {}
void Default_USB_EPDescCb() {}
void my_USB_DetectCB() {}
void my_USB_PrintCB() {}
struct {
    void setOnConfigDescCB(void (*)()) {}
    void setOnIfaceDescCb(void (*)()) {}
    void setOnHIDDevDescCb(void (*)()) {}
    void setOnEPDescCb(void (*)()) {}
    void init(int, void (*)(), void (*)()) { assert(nativeStarted); softStarted = true; }
} USH;
int USB_Pins_Config;
#include "SetupUsb.inc"

int main()
{
    memset(report, 0xff, sizeof(report));
    ClearKeyboardState();
    assert(!menuKey);
    for (uint8_t key : report) assert(!key);
    Setup_USB();
    assert(keysReady && nativeStarted && softStarted);
}
