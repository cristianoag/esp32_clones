#pragma once
#include <stdint.h>

void iwmInit(void);
void iwmWrite(unsigned int addr, unsigned int val);
unsigned int iwmRead(unsigned int addr);
void iwmSetHeadSel(int s);
void iwmStep(unsigned cycles);
int iwmInsert(const uint8_t *image, unsigned size);
void iwmEject(void);
int iwmInserted(void);
const uint8_t *iwmImage(void);
int iwmTakeActivity(void);
