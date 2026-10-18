/* TurboR A4/A5 behavior follows openMSX MSXTurboRPCM.
 * There is no physical microphone input on the shared board. */
#include "A1stInternal.h"
#include "Board.h"
#include "DAC.h"
#include "DeviceManager.h"
#include "IoPort.h"

static DAC* dac;
static UInt8 sample, status;
static UInt32 reference;
static BoardTimer* buffered;

static void output(void* ref, UInt32 time)
{
    (void)ref; (void)time;
    dacWrite(dac, DAC_CH_MONO, sample);
}
static UInt8 readPcm(void* ref, UInt16 port)
{
    (void)ref;
    if (!(port & 1))
        return ((UInt64)(UInt32)(boardSystemTime() - reference) * 15700 / boardFrequency()) & 3;
    return (status & 0x1f) | (0x80 >= sample ? 0x80 : 0);
}
static void writePcm(void* ref, UInt16 port, UInt8 value)
{
    (void)ref;
    if (!(port & 1)) {
        reference = boardSystemTime();
        sample = value;
        if (status & 2) {
            if (status & 1)
                boardTimerAdd(buffered, reference + boardFrequency() / 15700);
            else output(NULL, reference);
        }
    } else {
        UInt8 changed = status ^ value;
        status = value & 0x1f;
        mixerSetEnable(boardGetMixer(), status & 2);
        if ((changed & 1) && !(status & 1)) {
            boardTimerRemove(buffered);
            output(NULL, boardSystemTime());
        }
    }
}
static void resetPcm(void* ref)
{
    (void)ref;
    sample = 0x80; status = 0; reference = boardSystemTime();
    boardTimerRemove(buffered);
}
static void destroyPcm(void* ref)
{
    (void)ref;
    boardTimerDestroy(buffered);
    dacDestroy(dac);
    ioPortUnregister(0xa4);
    ioPortUnregister(0xa5);
}
void a1stPcmCreate(void)
{
    DeviceCallbacks callbacks = { destroyPcm, resetPcm, NULL, NULL };
    dac = dacCreate(boardGetMixer(), DAC_MONO);
    buffered = boardTimerCreate(output, NULL);
    deviceManagerRegister(ROM_TURBORPCM, &callbacks, NULL);
    ioPortRegister(0xa4, readPcm, writePcm, NULL);
    ioPortRegister(0xa5, readPcm, writePcm, NULL);
    resetPcm(NULL);
}
