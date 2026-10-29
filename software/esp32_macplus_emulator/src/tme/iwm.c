/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include "iwm.h"
#include <stdio.h>

/*
 * Sony drive sense/command behavior and GCR encoding adapted from MAME:
 * Copyright (c) Olivier Galibert, R. Belmont, Nathan Woods, Raphael Nabet.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define IWM_CA0		(1<<0)
#define IWM_CA1		(1<<1)
#define IWM_CA2		(1<<2)
#define IWM_LSTRB	(1<<3)
#define IWM_ENABLE	(1<<4)
#define IWM_SELECT	(1<<5)
#define IWM_Q6		(1<<6)
#define IWM_Q7		(1<<7)

static unsigned iwmLines, iwmModeReg, iwmHeadSel;
static const uint8_t *disk;
static unsigned diskSize, cylinder, direction, motor, changed, activity, writeReported;
static uint32_t clockCycles;
static uint8_t trackData[12 * 800];
static unsigned trackLength, trackPosition, cachedCylinder, cachedHead;

static const uint8_t gcr[64] = {
	0x96,0x97,0x9a,0x9b,0x9d,0x9e,0x9f,0xa6,
	0xa7,0xab,0xac,0xad,0xae,0xaf,0xb2,0xb3,
	0xb4,0xb5,0xb6,0xb7,0xb9,0xba,0xbb,0xbc,
	0xbd,0xbe,0xbf,0xcb,0xcd,0xce,0xcf,0xd3,
	0xd6,0xd7,0xd9,0xda,0xdb,0xdc,0xdd,0xde,
	0xdf,0xe5,0xe6,0xe7,0xe9,0xea,0xeb,0xec,
	0xed,0xee,0xef,0xf2,0xf3,0xf4,0xf5,0xf6,
	0xf7,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

static void emit(unsigned value) { trackData[trackLength++] = value; }

static void encodeThree(uint8_t a, uint8_t b, uint8_t c, unsigned count)
{
	emit(gcr[((a >> 2) & 0x30) | ((b >> 4) & 0x0c) | (c >> 6)]);
	emit(gcr[a & 63]);
	emit(gcr[b & 63]);
	if (count == 4) emit(gcr[c & 63]);
}

static void buildTrack(void)
{
	const unsigned heads = diskSize == 800 * 1024 ? 2 : 1;
	const unsigned sectors = 12 - cylinder / 16;
	unsigned block = 0;
	for (unsigned track = 0; track < cylinder; ++track) block += (12 - track / 16) * heads;
	block += iwmHeadSel * sectors;
	trackLength = trackPosition = 0;
	for (unsigned sector = 0; sector < sectors; ++sector) {
		const unsigned begin = trackLength;
		const uint8_t *data = disk + (block + sector) * 512;
		const unsigned side = (cylinder >> 6) | (iwmHeadSel << 5);
		const unsigned format = heads == 2 ? 0x22 : 0x02;
		for (unsigned i = 0; i < 36; ++i) emit(0xff);
		emit(0xd5); emit(0xaa); emit(0x96);
		emit(gcr[cylinder & 63]); emit(gcr[sector]);
		emit(gcr[side]); emit(gcr[format]);
		emit(gcr[(cylinder ^ sector ^ side ^ format) & 63]);
		emit(0xde); emit(0xaa); emit(0xff);
		for (unsigned i = 0; i < 5; ++i) emit(0xff);
		emit(0xd5); emit(0xaa); emit(0xad); emit(gcr[sector]);
		uint8_t ca = 0, cb = 0, cc = 0;
		for (unsigned i = 0; i < 175; ++i) {
			const unsigned pos = i * 3;
			uint8_t a = pos < 12 ? 0 : data[pos - 12];
			uint8_t b = pos + 1 < 12 ? 0 : data[pos + 1 - 12];
			uint8_t c = i == 174 || pos + 2 < 12 ? 0 : data[pos + 2 - 12];
			cc = (uint8_t)((cc << 1) | (cc >> 7));
			const unsigned sumA = ca + a + (cc & 1);
			ca = (uint8_t)sumA;
			a ^= cc;
			const unsigned sumB = cb + b + (sumA >> 8);
			cb = (uint8_t)sumB;
			b ^= ca;
			if (i != 174) cc = (uint8_t)(cc + c + (sumB >> 8));
			c ^= cb;
			encodeThree(a, b, c, i == 174 ? 3 : 4);
		}
		encodeThree(ca, cb, cc, 4);
		emit(0xde); emit(0xaa); emit(0xff); emit(0xff);
		while (trackLength - begin < 800) emit(0xff);
	}
	cachedCylinder = cylinder;
	cachedHead = iwmHeadSel;
}

void iwmEject(void)
{
	disk = NULL;
	diskSize = trackLength = trackPosition = motor = activity = 0;
	changed = 1;
}

int iwmInsert(const uint8_t *image, unsigned size)
{
	if (!image || disk || (size != 400 * 1024 && size != 800 * 1024)) return 0;
	disk = image;
	diskSize = size;
	trackLength = trackPosition = activity = writeReported = 0;
	changed = 1;
	return 1;
}

int iwmInserted(void) { return disk != NULL; }
const uint8_t *iwmImage(void) { return disk; }
int iwmTakeActivity(void) { const unsigned value = activity; activity = 0; return value; }
void iwmStep(unsigned cycles) { clockCycles += cycles; }

void iwmInit(void) {
	iwmLines = iwmModeReg = iwmHeadSel = 0;
	cylinder = direction = motor = clockCycles = writeReported = 0;
	iwmEject();
}

static void driveCommand(void)
{
	const unsigned command = (iwmLines & 7) | (iwmHeadSel << 3);
	switch (command) {
	case 0: direction = 0; break;
	case 1:
		if (direction) { if (cylinder) --cylinder; }
		else if (cylinder < 79) ++cylinder;
		trackLength = 0;
		break;
	case 2: motor = 1; break;
	case 4: direction = 1; break;
	case 6: motor = 0; break;
	case 7: iwmEject(); break;
	case 12: changed = 0; break;
	default: break;
	}
}

static void iwmAccess(unsigned int addr) {
	const unsigned previous = iwmLines;
	if (addr&1) {
		iwmLines|=(1<<(addr>>1));
	} else {
		iwmLines&=~(1<<(addr>>1));
	}
	if ((iwmLines & IWM_ENABLE) && !(iwmLines & IWM_SELECT) &&
	    (iwmLines & IWM_LSTRB) && !(previous & IWM_LSTRB)) driveCommand();
}

void iwmWrite(unsigned int addr, unsigned int val) {
	iwmAccess(addr);
	int reg=iwmLines&(IWM_Q7|IWM_Q6);
	if (reg == 0xc0 && (addr & 1)) {
		if (!(iwmLines & IWM_ENABLE)) iwmModeReg = val & 31;
		else if (disk && !writeReported) {
			printf("MACPLUS: floppy is read-only; write rejected.\n");
			writeReported = 1;
		}
	}
}

void iwmSetHeadSel(int s) {
	iwmHeadSel=s != 0;
}

static unsigned sense(void)
{
	if (!(iwmLines & IWM_ENABLE) || (iwmLines & IWM_SELECT)) return 1;
	switch ((iwmLines & 7) | (iwmHeadSel << 3)) {
	case 0: return direction;
	case 1: return 1;
	case 2: return !motor;
	case 3: return changed;
	case 5: return 0;
	case 6: return 1;
	case 7: return 0;
	case 8: return !disk;
	case 9: return 0;
	case 10: return cylinder != 0;
	case 11: {
		static const unsigned rpm[5] = {394,429,472,525,590};
		return disk && motor ? (unsigned)(((uint64_t)clockCycles * rpm[cylinder / 16] * 2 / 7833600) & 1) : 0;
	}
	case 14: return !disk || !motor;
	case 15: return 1;
	default: return 0;
	}
}

unsigned int iwmRead(unsigned int addr) {
	iwmAccess(addr);
	switch (iwmLines & (IWM_Q7 | IWM_Q6)) {
	case 0:
		if (!(iwmLines & IWM_ENABLE)) return 0xff;
		if ((iwmLines & IWM_SELECT) || !disk || !motor ||
		    (diskSize == 400 * 1024 && iwmHeadSel)) return 0;
		if (!trackLength || cachedCylinder != cylinder || cachedHead != iwmHeadSel) buildTrack();
		activity = 1;
		const unsigned value = trackData[trackPosition++];
		if (trackPosition == trackLength) trackPosition = 0;
		return value;
	case IWM_Q6:
		return (sense() ? 0x80 : 0) | (iwmLines & IWM_ENABLE ? 0x20 : 0) | iwmModeReg;
	case IWM_Q7: return 0xc0;
	default: return 0xff;
	}
}
