"""Approximate Waveshare ESP32-P4-Core-DEV-KIT 3D model for KiCad.

Requires cadquery==2.8.0. Run this file to generate ESP32-P4-Core-DEV-KIT.step
beside it.

Units: mm. Footprint frame: X=0, Y=0 at the centre of the 2 x 20 header
pattern, +X to the right and +Y toward the USB-C connectors (KiCad mirrors Y
for footprint space). Z=0 is the top surface of the carrier PCB.

Dimensions taken from the Waveshare documentation
(https://docs.waveshare.com/ESP32-P4-Core-DEV-KIT):
  PCB 28.62 x 58.00, pin pitch 2.54, first pin 4.87 from each PCB end.

Estimated, NOT manufacturer-certified:
  Header row separation 25.4, PCB thickness 1.6, 2.5 high header spacers,
  0.64 square pins, connector, button and module envelopes.

Use for visualization and fit checks, not for enclosure machining.
"""

from pathlib import Path

import cadquery as cq

HERE = Path(__file__).resolve().parent
STEM = "ESP32-P4-Core-DEV-KIT"
PCB_W, PCB_L, PCB_T = 28.62, 58.0, 1.6
PITCH, ROW_X = 2.54, 12.7
SPACER_H = 2.5
PCB_BOTTOM = SPACER_H
PCB_TOP = PCB_BOTTOM + PCB_T
PIN_Y = tuple(-24.13 + i * PITCH for i in range(20))[::-1]

BOARD = (0.05, 0.20, 0.55)
BLACK = (0.10, 0.11, 0.12)
GOLD = (0.78, 0.65, 0.33)
SILVER = (0.78, 0.78, 0.80)
WHITE = (0.93, 0.93, 0.90)
GREY = (0.35, 0.35, 0.38)


def box(cx, cy, z0, sx, sy, sz):
    return cq.Workplane("XY").box(sx, sy, sz, centered=(True, True, False)).translate((cx, cy, z0)).val()


def build():
    parts = []
    pcb = cq.Workplane("XY").box(PCB_W, PCB_L, PCB_T, centered=(True, True, False)).translate((0, 0, PCB_BOTTOM))
    holes = [(sx * 12.9, sy * 27.5) for sx in (-1, 1) for sy in (-1, 1)]
    pcb = pcb.cut(
        cq.Workplane("XY").pushPoints(holes).circle(1.1).extrude(PCB_T).translate((0, 0, PCB_BOTTOM))
    )
    parts.append(("carrier_pcb", pcb.val(), BOARD))

    spacers, pins = [], []
    for x in (-ROW_X, ROW_X):
        spacers.append(box(x, 0, 0, PITCH, PITCH * 20, SPACER_H))
        for y in PIN_Y:
            pins.append(box(x, y, -3.0, 0.64, 0.64, 3.0 + PCB_TOP + 1.0))
    parts.append(("header_spacers", cq.Compound.makeCompound(spacers), BLACK))
    parts.append(("header_pins", cq.Compound.makeCompound(pins), GOLD))

    usb = [box(x, 29.4 - 7.3 / 2, PCB_TOP, 9.0, 7.3, 3.2) for x in (-6.8, 6.8)]
    parts.append(("usb_c_shells", cq.Compound.makeCompound(usb), SILVER))

    parts.append(("jst_mx1.25_4pin", box(-0.5, 19.0, PCB_TOP, 7.5, 4.5, 4.0), WHITE))
    buttons = [box(x, 16.0, PCB_TOP, 4.0, 3.0, 2.0) for x in (-9.5, 9.5)]
    parts.append(("boot_reset_buttons", cq.Compound.makeCompound(buttons), GREY))
    fpc = [box(0, y, PCB_TOP, 15.0, 3.0, 1.5) for y in (7.5, 0.0)]
    parts.append(("fpc_display_camera", cq.Compound.makeCompound(fpc), BLACK))

    parts.append(("module_substrate", box(0, -16.5, PCB_TOP, 20.0, 24.0, 0.8), BOARD))
    parts.append(("module_shield", box(0, -16.5, PCB_TOP + 0.8, 16.2, 19.9, 2.2), SILVER))
    return parts


def export_step(parts):
    assembly = cq.Assembly(name=STEM)
    for name, shape, color in parts:
        assembly.add(shape, name=name, color=cq.Color(*color))
    path = HERE / f"{STEM}.step"
    assembly.export(str(path))
    return path


if __name__ == "__main__":
    print(export_step(build()))
