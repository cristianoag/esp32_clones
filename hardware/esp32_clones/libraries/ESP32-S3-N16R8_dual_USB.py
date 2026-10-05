"""Approximate 44-pin ESP32-S3-WROOM-1-N16R8 dual-USB-C development board.

Reconstructed from the user's dimensioned top photo and populated-header photo.
Requires cadquery==2.8.0 and sibling pushbutton_12x6_7.py for shared CAD helpers.
Run this file to generate the STEP assembly and four-view PNG beside it.
No PCB footprint or existing model assignment is changed.

Units: mm. X=0 at carrier PCB USB edge, +X toward antenna; Y=0 at board
centerline; Z=0 at carrier PCB bottom, +Z toward components.

Dimensioned photo / user specification:
  Carrier PCB 57.15 x 27.94 (excludes overhanging antenna).
  Two rows of 22 male pins, pitch 2.54, longitudinal center span 53.34.
  Header row center separation 25.4.
  Centers X=1.905 + n*2.54, Y=+/-12.7 (symmetric edge margins assumed).

Module envelope: 25.5 x 18 x 3.1, oriented lengthwise on the carrier.
Reference: Espressif ESP32-S3-WROOM-1 / WROOM-1U datasheet:
https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.html
User product image: https://ssdielect.com/15118-large_default/esp32-s3-wroom-1-n16r8.jpg

Estimated, NOT manufacturer-certified:
  Carrier PCB thickness 1.6; mounting-hole diameter 1.0, pad diameter 1.7.
  Male pins 0.64 square, 6 below a 2.5-high spacer, 1 above carrier top.
  Yellow header spacers and black carrier match the user's second image.
  Module X=38.15..63.65: 6.5 antenna overhang beyond carrier, no extra keepout.
  Module substrate 0.8 thick; shield envelope and 40 visible lands simplified.
  USB-C shells 7.5 deep x 9 wide x 3.2 high, front X=-0.4; centers Y=+/-6.35.
  Buttons, LED, ICs, passives, connector contacts and their placement/height
  are photo-based approximations. USB ports have open mouths and tongues.
  Silk-screen legends, routing, solder fillets, antenna copper and concealed
  components are omitted. Pin numbering/electrical connectivity is not inferred.

Use for visualization and preliminary fit checks, not connector certification,
production footprint design or final enclosure machining without measurement.
"""

from math import isclose
from pathlib import Path

import cadquery as cq
import vtk

from pushbutton_12x6_7 import METAL, actor, box, check_close


HERE = Path(__file__).resolve().parent
STEM = "ESP32-S3-N16R8_dual_USB"
PCB_LENGTH, PCB_WIDTH, PCB_THICKNESS = 57.15, 27.94, 1.6
PITCH, ROW_SPACING = 2.54, 25.4
PIN_X = tuple(1.905 + index * PITCH for index in range(22))
ROW_Y = (-ROW_SPACING / 2, ROW_SPACING / 2)
GOLD = (0.78, 0.65, 0.33)
BLACK = (0.10, 0.11, 0.12)
BOARD = (0.12, 0.15, 0.14)


def cylinder(x, y, z, radius, height):
    return cq.Solid.makeCylinder(radius, height, cq.Vector(x, y, z))


def compound(shapes):
    return cq.Compound.makeCompound(shapes)


def usb_parts(y, index):
    x0, x1 = -0.4, 7.1
    outer = (cq.Workplane(obj=box(x0, y - 4.5, 1.6, x1, y + 4.5, 4.8))
             .edges("|X").fillet(1.35).val())
    cavity = (cq.Workplane(obj=box(x0 - 0.1, y - 4.23, 1.87,
                                  x1 - 0.4, y + 4.23, 4.53))
              .edges("|X").fillet(1.08).val())
    shell = outer.cut(cavity)
    tongue = box(0.4, y - 3.2, 2.9, 6.8, y + 3.2, 3.5)
    contacts = []
    for contact in range(12):
        cy = y + (contact - 5.5) * 0.5
        for z in (2.865, 3.5):
            contacts.append(box(0.8, cy - 0.11, z, 4.8, cy + 0.11, z + 0.035))
    return [
        (f"USB_C_{index}_shell", shell, METAL),
        (f"USB_C_{index}_tongue", tongue, BLACK),
        (f"USB_C_{index}_contacts_visual_only", compound(contacts), GOLD),
    ]


def build():
    pcb = box(0, -PCB_WIDTH / 2, 0, PCB_LENGTH, PCB_WIDTH / 2, PCB_THICKNESS)
    pads, pins = [], []
    for row, y in enumerate(ROW_Y):
        for index, x in enumerate(PIN_X):
            pcb = pcb.cut(cylinder(x, y, -0.1, 0.5, 1.8))
            for z in (-0.035, PCB_THICKNESS):
                pads.append(cylinder(x, y, z, 0.85, 0.035)
                            .cut(cylinder(x, y, z - 0.01, 0.5, 0.055)))
            pin = (cq.Workplane(obj=box(x - 0.32, y - 0.32, -8.5,
                                       x + 0.32, y + 0.32, 2.6))
                   .faces(">Z or <Z").edges().chamfer(0.12).val())
            pins.append((f"header_row_{row}_pin_{index:02d}", pin, METAL))
    parts = [("carrier_PCB", pcb, BOARD), ("header_annular_pads", compound(pads), GOLD)]
    for row, y in enumerate(ROW_Y):
        spacer = box(PIN_X[0] - 1.27, y - 1.27, -2.5,
                     PIN_X[-1] + 1.27, y + 1.27, 0)
        for x in PIN_X:
            spacer = spacer.cut(box(x - 0.34, y - 0.34, -2.6,
                                    x + 0.34, y + 0.34, 0.1))
        parts.append((f"header_row_{row}_spacer", spacer, (0.90, 0.74, 0.16)))
    parts.extend(pins)
    module = box(38.15, -9, 1.6, 63.65, 9, 2.4)
    shield_outer = (cq.Workplane(obj=box(39.25, -8.2, 2.4, 56.95, 8.2, 4.7))
                    .edges("|Z").fillet(0.3).val())
    shield = shield_outer.cut(box(39.45, -8, 2.39, 56.75, 8, 4.5))
    parts.extend([
        ("WROOM_1_module_substrate_and_antenna", module, (0.09, 0.10, 0.10)),
        ("WROOM_1_RF_shield", shield, (0.71, 0.70, 0.64)),
    ])
    lands = []
    for x in (39.65 + index * 1.27 for index in range(14)):
        for y0, y1 in ((-9.1, -8.5), (8.5, 9.1)):
            lands.append(box(x - 0.35, y0, 1.6, x + 0.35, y1, 2.45))
    for y in ((index - 5.5) * 1.27 for index in range(12)):
        lands.append(box(38.05, y - 0.35, 1.6, 38.65, y + 0.35, 2.45))
    parts.append(("module_lands_visual_only", compound(lands), METAL))
    for index, y in enumerate((-6.35, 6.35)):
        parts.extend(usb_parts(y, index))
    for name, y in (("BOOT", -8.1), ("RESET", -3.0)):
        x = 30.6
        parts.extend([
            (f"{name}_switch_body", box(x - 2.25, y - 1.6, 1.6,
                                       x + 2.25, y + 1.6, 2.6), BLACK),
            (f"{name}_switch_cover", box(x - 2.15, y - 1.5, 2.6,
                                        x + 2.15, y + 1.5, 2.9), METAL),
            (f"{name}_button", cylinder(x, y, 2.9, 0.9, 0.6), BLACK),
        ])
    parts.extend([
        ("RGB_LED_package", box(15.9, 2.5, 1.6, 20.9, 7.5, 3.1),
         (0.86, 0.87, 0.78)),
        ("RGB_LED_lens", cylinder(18.4, 5, 3.1, 1.8, 0.12), (0.88, 0.92, 0.82)),
        ("USB_UART_IC", box(12, -9.4, 1.6, 16, -5.4, 2.5), BLACK),
        ("regulator_package", box(23, 2.4, 1.6, 26.6, 6.8, 3.1), BLACK),
    ])
    ic_leads = []
    for i in range(7):
        p = (i - 3) * 0.5
        ic_leads.extend([
            box(11.6, -7.4 + p - 0.12, 1.6, 12, -7.4 + p + 0.12, 1.8),
            box(16, -7.4 + p - 0.12, 1.6, 16.4, -7.4 + p + 0.12, 1.8),
            box(14 + p - 0.12, -9.8, 1.6, 14 + p + 0.12, -9.4, 1.8),
            box(14 + p - 0.12, -5.4, 1.6, 14 + p + 0.12, -5, 1.8),
        ])
    parts.append(("USB_UART_leads_visual_only", compound(ic_leads), METAL))
    for i, (x, y, length, width, height) in enumerate((
            (30.3, 7.0, 4.7, 2.4, 1.5), (30.3, 3.2, 4.7, 2.4, 1.5),
            (20.0, -3.2, 2.0, 1.0, 0.7), (20.0, -5.8, 2.0, 1.0, 0.7),
            (20.0, -8.0, 2.0, 1.0, 0.7), (11.5, 5.0, 2.0, 1.0, 0.7),
            (11.5, 2.5, 2.0, 1.0, 0.7), (11.5, -0.5, 2.0, 1.0, 0.7),
            (24.0, -3.0, 1.6, 0.8, 0.6), (24.0, -5.5, 1.6, 0.8, 0.6),
            (24.0, -8.0, 1.6, 0.8, 0.6))):
        parts.append((f"passive_{i}", box(x - length / 2, y - width / 2, 1.6,
                                         x + length / 2, y + width / 2, 1.6 + height),
                      (0.35, 0.30, 0.23)))
        ends = [box(x0, y - width / 2, 1.6, x0 + 0.25, y + width / 2, 1.65 + height)
                for x0 in (x - length / 2, x + length / 2 - 0.25)]
        parts.append((f"passive_{i}_terminations", compound(ends), METAL))
    return parts


def validate(parts):
    for name, shape, _ in parts:
        if not shape.isValid() or not shape.Solids() or shape.Volume() <= 0:
            raise ValueError(f"Invalid geometry: {name}")
    shapes = {name: shape for name, shape, _ in parts}
    pcb = shapes["carrier_PCB"]
    bounds = pcb.BoundingBox()
    for actual, expected in zip((bounds.xlen, bounds.ylen, bounds.zlen), (57.15, 27.94, 1.6)):
        check_close(actual, expected, "carrier PCB dimensions")
    pins = [(name, shape) for name, shape, _ in parts if "_pin_" in name]
    if len(pins) != 44:
        raise ValueError(f"Expected 44 header pins, found {len(pins)}")
    check_close(PIN_X[-1] - PIN_X[0], 53.34, "header longitudinal span")
    for row, y in enumerate(ROW_Y):
        previous_x = None
        for index, x in enumerate(PIN_X):
            pin = shapes[f"header_row_{row}_pin_{index:02d}"]
            b = pin.BoundingBox()
            check_close(b.center.x, x, "pin X position")
            check_close(b.center.y, y, "pin Y position")
            check_close(b.xlen, 0.64, "square pin width")
            check_close(b.ylen, 0.64, "square pin thickness")
            check_close(b.zmin, -8.5, "pin tip below PCB")
            check_close(b.zmax, 2.6, "pin tip above PCB")
            if previous_x is not None:
                check_close(b.center.x - previous_x, 2.54, "actual pin pitch")
            previous_x = b.center.x
            if pin.intersect(pcb).Volume() > 1e-7:
                raise ValueError(f"Pin intersects PCB rather than passing through hole: {index}")
        if len(shapes[f"header_row_{row}_spacer"].Solids()) != 1:
            raise ValueError("Header spacer must be a single solid")
    row_centers = [shapes[f"header_row_{row}_pin_00"].BoundingBox().center.y for row in range(2)]
    check_close(row_centers[1] - row_centers[0], 25.4, "actual row spacing")
    module = shapes["WROOM_1_module_substrate_and_antenna"].BoundingBox()
    shield = shapes["WROOM_1_RF_shield"].BoundingBox()
    check_close(module.xlen, 25.5, "module length")
    check_close(module.ylen, 18, "module width")
    check_close(shield.zmax - module.zmin, 3.1, "module total height")
    check_close(module.xmax - PCB_LENGTH, 6.5, "estimated antenna overhang")
    for index, y in enumerate((-6.35, 6.35)):
        shell = shapes[f"USB_C_{index}_shell"]
        mouth = box(-0.41, y - 3, 2.1, 0.1, y + 3, 4.3)
        if shell.intersect(mouth).Volume() > 1e-7:
            raise ValueError(f"USB-C {index} mouth is blocked")
    print("PASS 44 pins: 2.54 mm pitch, 53.34 mm span, 25.4 mm row spacing")
    print("PASS PCB dimensions, pin/hole clearance, module envelope and open USB mouths")


def export_step(parts):
    assembly = cq.Assembly(name=STEM)
    for name, shape, color in parts:
        assembly.add(shape, name=name, color=cq.Color(*color))
    path = HERE / f"{STEM}.step"
    assembly.export(str(path))
    imported = cq.importers.importStep(str(path)).val()
    count = sum(len(shape.Solids()) for _, shape, _ in parts)
    if not imported.isValid() or len(imported.Solids()) != count:
        raise ValueError("STEP round-trip failed validity / solid-count checks")
    bounds = imported.BoundingBox()
    for actual, expected in zip((bounds.xlen, bounds.ylen, bounds.zlen), (64.05, 27.94, 13.3)):
        check_close(actual, expected, "STEP total envelope")
    check_close(bounds.zmin, -8.5, "STEP lowest pin")
    check_close(bounds.zmax, 4.8, "STEP tallest component")
    if not isclose(imported.Volume(), sum(s.Volume() for _, s, _ in parts), rel_tol=1e-6):
        raise ValueError("STEP volume changed during export")
    print(f"PASS {path.name}: {count} valid solids; "
          f"{bounds.xlen:.3f} x {bounds.ylen:.3f} x {bounds.zlen:.3f} mm")


def preview(parts):
    window = vtk.vtkRenderWindow()
    window.SetOffScreenRendering(True)
    window.SetSize(1800, 1300)
    views = (
        ((-40, -65, 70), (0, 0, 1), "TOP / USB END - 44 male pins, 2.54 mm pitch"),
        ((32, 0, 100), (0, 1, 0), "TOP - 57.15 x 27.94 mm carrier PCB"),
        ((-30, -65, -60), (0, 0, 1), "UNDERSIDE - 25.4 mm header row spacing"),
        ((32, -100, -1), (0, 0, 1), "SIDE - estimated 6.5 mm antenna overhang"),
    )
    for index, (position, up, title) in enumerate(views):
        renderer = vtk.vtkRenderer()
        column, row = index % 2, 1 - index // 2
        renderer.SetViewport(column / 2, row / 2, (column + 1) / 2, (row + 1) / 2)
        renderer.SetBackground(0.94, 0.95, 0.97)
        window.AddRenderer(renderer)
        for _, shape, color in parts:
            renderer.AddActor(actor(shape, color))
        label = vtk.vtkTextActor()
        label.SetInput("ESP32-S3 N16R8 / " + title + "\nApproximate exterior - dimensions in mm")
        label.SetPosition(20, 20)
        label.GetTextProperty().SetFontSize(17)
        label.GetTextProperty().SetColor(0.16, 0.18, 0.22)
        renderer.AddViewProp(label)
        camera = renderer.GetActiveCamera()
        camera.SetPosition(*position)
        camera.SetFocalPoint(31.5, 0, -1)
        camera.SetViewUp(*up)
        camera.ParallelProjectionOn()
        camera.SetParallelScale(31)
        renderer.ResetCameraClippingRange()
    window.Render()
    capture = vtk.vtkWindowToImageFilter()
    capture.SetInput(window)
    capture.Update()
    writer = vtk.vtkPNGWriter()
    writer.SetFileName(str(HERE / f"{STEM}_preview.png"))
    writer.SetInputConnection(capture.GetOutputPort())
    writer.Write()
    window.Finalize()


def main():
    parts = build()
    validate(parts)
    export_step(parts)
    preview(parts)


if __name__ == "__main__":
    main()
