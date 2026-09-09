// Reuse the original transport, including its attribution, without the CP400
// CPU ticker, LED GPIO, timer ownership, or boot-keyboard wrapper.
#define MSX_SOFT_USB 1
#include "../../../../esp32_cp400_emulator/lib/SoftUSB/usb_host.c"
