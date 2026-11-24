#include "MacScsi.h"
#include "MacPlatform.h"
#include <string.h>

namespace {
uint8_t sense = 0;

int command(SCSITransferData *data, unsigned op, unsigned count, unsigned lba, void *context)
{
    MacDisk &disk = *static_cast<MacDisk *>(context);
    data->cmd[0] = data->msg[0] = 0;
    char error[160] = {};
    unsigned returned = 0;
    if (op == 0x08 || op == 0x28 || op == 0x0a || op == 0x2a) {
        const bool write = op == 0x0a || op == 0x2a;
        if (!disk.transfer(write, lba, count, data->data, sizeof(data->data), error, sizeof(error))) {
            sense = write && disk.readOnly() ? 7 : 3;
            data->cmd[0] = 2;
            macReportError(error);
            return 0;
        }
        returned = write ? 0 : count * 512;
    } else if (op == 0x12) {
        const uint8_t inquiry[] = {
            0,0,1,1,31,0,0,0,'A','P','P','L','E',' ',' ',' ',
            '2','0','S','C',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
            '1','.','0',' '
        };
        memset(data->data, 0, 96);
        memcpy(data->data, inquiry, sizeof(inquiry));
        returned = count < sizeof(inquiry) ? count : sizeof(inquiry);
    } else if (op == 0x25) {
        const uint32_t last = disk.blocks() - 1;
        for (unsigned i = 0; i < 4; ++i) data->data[i] = last >> (24 - i * 8);
        data->data[4] = data->data[5] = data->data[7] = 0;
        data->data[6] = 2;
        returned = 8;
    } else if (op == 0x03) {
        memset(data->data, 0, 18);
        data->data[0] = 0x70;
        data->data[2] = sense;
        data->data[7] = 10;
        returned = count < 18 ? count : 18;
        sense = 0;
    } else if (op == 0x1a) {
        memset(data->data, 0, 12);
        data->data[0] = 11;
        data->data[2] = disk.readOnly() ? 0x80 : 0;
        data->data[3] = 8;
        const uint32_t blocks = disk.blocks();
        data->data[5] = blocks >> 16;
        data->data[6] = blocks >> 8;
        data->data[7] = blocks;
        data->data[10] = 2;
        returned = count < 12 ? count : 12;
    } else if (op != 0x00 && op != 0x01 && op != 0x1b && op != 0x1e &&
               op != 0x35 && op != 0x15) {
        sense = 5;
        data->cmd[0] = 2;
        snprintf(error, sizeof(error), "Unsupported SCSI command %02X.", op);
        macReportError(error);
    }
    return returned;
}
SCSIDevice device;
}

void MacAttachScsi(MacDisk *disk)
{
    sense = 0;
    device.scsiCmd = command;
    device.arg = disk;
    ncrRegisterDevice(6, disk ? &device : nullptr);
}
