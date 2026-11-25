"""Approximate switch exterior from the second user-supplied dimension drawing.

Requires cadquery==2.8.0 and the sibling pushbutton_12x6_7.py for shared
geometry, validation and rendering helpers. Run this file to regenerate the
STEP assembly and four-view PNG beside it. Existing PCB files are not changed.
The upper mounting lug and screw boss are omitted to match the user's physical
switch. The original filenames are retained for compatibility.

All dimensions are mm. Origin: housing rear, width center, PCB seating plane.
+X points toward the actuator; +Z points away from the PCB.

Drawing dimensions used:
  Housing envelope: 12.2 long x 7.7 wide x 10 high.
  Actuator projection: 12.9; end tip: 2.7 long x 3.3 square.
  Main shaft: 2.5 square, interpreting the top-view width callout.
  Six terminals: 1 wide x 0.4 thick, longitudinal pitch 3.2 (6.4 span),
  transverse pitch 3; housing top to terminal tips: 13.3.
  Mounting-tab longitudinal thickness: 0.6.

Undimensioned details / assumptions:
  Actuator axis Z=5; terminal X centers 3.0, 6.2, 9.4.
  Four mounting tabs centered at X=0.3/11.9, Y=+/-2.9; 0.6 wide,
  extending 1.2 below the seating plane.
  Shaft flange: 5.8 square x 0.6 thick, rear face X=17.9.
  Neck: 2.5 square x 1 long. Spring: four turns, mean radius 2.35,
  wire diameter 0.4, idealized open ends.
  Metal shell 0.35 thick, plastic base 1.2 thick. Reliefs, colors and corner
  radii are cosmetic. Internal contacts and latch mechanism are omitted.
  The displayed actuator position is modeled; travel is NOT specified.
  No electrical pin numbering or PCB footprint is inferred from the circuit.

For visualization and preliminary clearance checks, not manufacture or
production footprint validation. Verify undimensioned features on the part.
"""

from math import isclose
from pathlib import Path

import cadquery as cq
import vtk

from pushbutton_12x6_7 import ACTUATOR, METAL, PLASTIC, actor, box, check_close


HERE = Path(__file__).resolve().parent
STEM = "pushbutton_12_2x7_7_lug"
AXIS_Z = 5.0


def shaft(x0, x1, size):
    return box(x0, -size / 2, AXIS_Z - size / 2,
               x1, size / 2, AXIS_Z + size / 2)


def build():
    outer = (cq.Workplane("XY").center(6.1, 0).rect(12.2, 7.7).extrude(9.2)
             .edges("|Z").fillet(0.35).translate((0, 0, 0.8)).val())
    shell = outer.cut(box(0.35, -3.5, 0.7, 11.85, 3.5, 9.65))
    shell = shell.cut(shaft(11.7, 12.3, 3.0))
    for y0, y1 in ((-4, -3.4), (3.4, 4)):
        for x0, x1 in ((1.3, 2.3), (3.7, 8.7), (10.1, 10.9)):
            shell = shell.cut(box(x0, y0, 0.7, x1, y1, 1.8))
    base = (cq.Workplane("XY").center(6.1, 0).rect(11.5, 6.9).extrude(1.2)
            .edges("|Z").fillet(0.25).val())
    tip = (cq.Workplane(obj=shaft(22.4, 25.1, 3.3))
           .faces(">X").edges().fillet(0.2).val())
    actuator = (shaft(11.2, 21.4, 2.5)
                .fuse(shaft(17.9, 18.5, 5.8))
                .fuse(shaft(18.5, 19.1, 4.2))
                .fuse(shaft(19.1, 21.4, 3.3))
                .fuse(shaft(21.4, 22.4, 2.5)).fuse(tip).clean())
    wire_radius = 0.2
    spring_length = 17.9 - 12.2 - 2 * wire_radius
    helix = cq.Wire.makeHelix(spring_length / 4, spring_length, 2.35)
    profile = cq.Plane(origin=helix.startPoint(), normal=helix.tangentAt(0))
    spring = (cq.Workplane(profile).circle(wire_radius)
              .sweep(cq.Workplane(obj=helix), isFrenet=True).val()
              .rotate((0, 0, 0), (0, 1, 0), 90)
              .translate((12.2 + wire_radius, 0, AXIS_Z)))
    parts = [
        ("shell", shell, METAL),
        ("base", base, PLASTIC),
        ("actuator", actuator, ACTUATOR),
        ("spring", spring, (0.42, 0.44, 0.47)),
    ]
    for column, x in enumerate((3.0, 6.2, 9.4)):
        for row, y in enumerate((-1.5, 1.5)):
            terminal = (cq.Workplane(obj=box(x - 0.5, y - 0.2, -3.3,
                                            x + 0.5, y + 0.2, 0.8))
                        .faces("<Z").edges().chamfer(0.12).val())
            parts.append((f"terminal_{column}_{row}", terminal, METAL))
    for end, x in enumerate((0.3, 11.9)):
        for side, y in enumerate((-2.9, 2.9)):
            tab = (cq.Workplane(obj=box(x - 0.3, y - 0.3, -1.2,
                                       x + 0.3, y + 0.3, 1.4))
                   .faces("<Z").edges().chamfer(0.1).val())
            parts.append((f"mounting_tab_{end}_{side}", tab, METAL))
    return parts


def validate(parts):
    for name, shape, _ in parts:
        if not shape.isValid() or len(shape.Solids()) != 1 or shape.Volume() <= 0:
            raise ValueError(f"Invalid solid: {name}")
    shapes = {name: shape for name, shape, _ in parts}
    shell = shapes["shell"].BoundingBox()
    check_close(shell.xlen, 12.2, "housing length")
    check_close(shell.ylen, 7.7, "housing width")
    check_close(shell.zmax, 10, "housing top")
    check_close(shapes["base"].BoundingBox().zmin, 0, "seating plane")
    check_close(shapes["actuator"].BoundingBox().xmax - shell.xmax,
                12.9, "actuator projection")
    tip = shapes["actuator"].intersect(box(22.4, -4, 0, 25.2, 4, 10))
    for actual, expected in zip(
            (tip.BoundingBox().xlen, tip.BoundingBox().ylen, tip.BoundingBox().zlen),
            (2.7, 3.3, 3.3)):
        check_close(actual, expected, "actuator tip dimensions")
    for name, shape, _ in parts:
        if shape.BoundingBox().zmax > 10 + 1e-5:
            raise ValueError(f"Unexpected projection above housing: {name}")
    for column in range(3):
        for row in range(2):
            bounds = shapes[f"terminal_{column}_{row}"].BoundingBox()
            check_close(bounds.xlen, 1, "terminal width")
            check_close(bounds.ylen, 0.4, "terminal thickness")
            check_close(bounds.center.x, 3 + column * 3.2, "terminal X pitch")
            check_close(bounds.center.y, -1.5 + row * 3, "terminal row pitch")
            check_close(shell.zmax - bounds.zmin, 13.3, "terminal tip height")


def export_step(parts):
    assembly = cq.Assembly(name=STEM)
    for name, shape, color in parts:
        assembly.add(shape, name=name, color=cq.Color(*color))
    path = HERE / f"{STEM}.step"
    assembly.export(str(path))
    imported = cq.importers.importStep(str(path)).val()
    if not imported.isValid() or len(imported.Solids()) != len(parts):
        raise ValueError(f"STEP round-trip validation failed: {path}")
    bounds = imported.BoundingBox()
    check_close(bounds.xlen, 25.1, "STEP overall length")
    check_close(bounds.ylen, 7.7, "STEP overall width")
    check_close(bounds.zmax, 10, "STEP top without mounting lug")
    check_close(bounds.zlen, 13.3, "STEP height including pins")
    expected_volume = sum(shape.Volume() for _, shape, _ in parts)
    if not isclose(imported.Volume(), expected_volume, rel_tol=1e-6):
        raise ValueError(f"STEP volume changed during export: {path}")
    print(f"PASS {path.name}: {len(parts)} valid solids; "
          f"{bounds.xlen:.3f} x {bounds.ylen:.3f} x {bounds.zlen:.3f} mm")
    print("PASS housing, actuator tip/projection, terminal dimensions and no upper lug")


def preview(parts):
    window = vtk.vtkRenderWindow()
    window.SetOffScreenRendering(True)
    window.SetSize(1600, 1200)
    views = (
        ((39, -38, 31), "SHOWN POSITION - 25.1 mm overall length"),
        ((35, -36, -24), "UNDERSIDE - six terminals, 3.2 x 3 mm pitch"),
        ((12, -45, 6), "SIDE - 12.9 mm actuator projection"),
        ((55, 0, 6), "FRONT - no upper mounting lug or screw boss"),
    )
    for index, (position, title) in enumerate(views):
        renderer = vtk.vtkRenderer()
        column, row = index % 2, 1 - index // 2
        renderer.SetViewport(column / 2, row / 2, (column + 1) / 2, (row + 1) / 2)
        renderer.SetBackground(0.94, 0.95, 0.97)
        window.AddRenderer(renderer)
        for _, shape, color in parts:
            renderer.AddActor(actor(shape, color))
        label = vtk.vtkTextActor()
        label.SetInput(title + "\nApproximate exterior model - dimensions in mm")
        label.SetPosition(20, 20)
        label.GetTextProperty().SetFontSize(17)
        label.GetTextProperty().SetColor(0.16, 0.18, 0.22)
        renderer.AddViewProp(label)
        camera = renderer.GetActiveCamera()
        camera.SetPosition(*position)
        camera.SetFocalPoint(12, 0, 6)
        camera.SetViewUp(0, 0, 1)
        camera.ParallelProjectionOn()
        camera.SetParallelScale(14.5)
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
