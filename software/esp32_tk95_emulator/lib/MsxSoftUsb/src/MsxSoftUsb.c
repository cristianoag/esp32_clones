// Use the locally vendored transport, including its attribution, without the CP400
// CPU ticker, LED GPIO, timer ownership, or boot-keyboard wrapper.
#define MSX_SOFT_USB 1
#include "../upstream/usb_host.c"
