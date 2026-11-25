#pragma once
#include "MacDisk.h"
extern "C" {
#include "tme/ncr.h"
}
void MacAttachScsi(MacDisk *disk);
