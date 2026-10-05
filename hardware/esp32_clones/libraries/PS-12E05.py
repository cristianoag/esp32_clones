"""PS-12E05 approximate exterior reconstructed from the user's drawing/photo.

Requires cadquery==2.8.0 and the sibling pushbutton_12x6_7.py for shared
geometry/validation/rendering helpers. Run this file to generate released,
latched and fully pressed STEP assemblies, plus a four-view PNG.

Units: mm. Origin: housing rear, terminal-row center, PCB seating plane.
+X points toward the actuator; +Z points away from the PCB.

Drawing dimensions used:
  Housing: 12.2 long x 5 wide x 8.3 high; actuator axis Z=4.7.
  Released total length 22.1; lock travel 2.5; full travel 3.5.
  Actuator tip: 2 long x 2.8 square; neck: 1 long x 2 square.
  Flange rear face: 4 behind the released tip.
  Three signal terminals: 0.5 wide x 0.4 thick, 2.5 pitch (5 span),
  first terminal center X=3.2; housing top to terminal tips 11.1.
  Mounting tabs: 0.8 wide x 0.6 thick; front tab center X=11.9;
  housing top to tab tips 11.6.

Assumptions / ambiguous details:
  The end-view 1.1 and 2.2 callouts are interpreted as mounting-tab center
  offsets from the signal row: rear tab Y=-1.1, front tab Y=+2.2.
  Rear tab X=0.4. Confirm tab positions/orientation before footprint use.
  Main shaft 2 square; flange 4.6 square x 0.5 thick; shoulder 2.8 square.
  Spring mean radius 1.85, wire diameter 0.4, four turns, idealized open ends.
  Shell 0.3 thick; plastic base 1.2 thick. Housing reliefs, stamped triangular
  opening, radii and colors are cosmetic, not dimensioned manufacturing data.
  No internal contacts, latch mechanism or electrical pin numbers are modeled.
  Latched/pressed assemblies show displacement only, not contact operation.

Suitable for visualization and preliminary enclosure checks, not manufacture
or production footprint validation. PCB files and other switch models are
not modified by this generator.
"""

from math import isclose
from pathlib import Path

import cadquery as cq
import vtk

from pushbutton_12x6_7 import METAL, PLASTIC, actor, box, check_close


HERE = Path(__file__).resolve().parent
STEM = "PS-12E05"
AXIS_Z = 4.7
STATES = {"released": 0.0, "latched": 2.5, "pressed": 3.5}


def shaft(x0, x1, size):
    return box(x0, -size / 2, AXIS_Z - size / 2,
               x1, size / 2, AXIS_Z + size / 2)


def build(displacement):
    if displacement not in STATES.values():
        raise ValueError(f"Unsupported actuator displacement: {displacement}")
    outer = (cq.Workplane("XY").center(6.1, 0).rect(12.2, 5).extrude(7.5)
             .edges("|Z").fillet(0.25).translate((0, 0, 0.8)).val())
    shell = outer.cut(box(0.3, -2.2, 0.7, 11.9, 2.2, 8))
    shell = shell.cut(shaft(11.8, 12.3, 2.6))
    stamp = (cq.Workplane("XY").workplane(offset=7.95)
             .polyline([(6.6, -1.1), (10.1, 0), (6.6, 1.1)])
             .close().extrude(0.5).val())
    shell = shell.cut(stamp)
    for y0, y1 in ((-2.6, -2.1), (2.1, 2.6)):
        for x0, x1 in ((1.3, 2.5), (4.6, 7.8), (9.1, 10.9)):
            shell = shell.cut(box(x0, y0, 0.7, x1, y1, 1.8))
    base = (cq.Workplane("XY").center(6.1, 0).rect(11.6, 4.3).extrude(1.2)
            .edges("|Z").fillet(0.2).val())
    tip = (cq.Workplane(obj=shaft(20.1, 22.1, 2.8))
           .faces(">X").edges().fillet(0.2).val())
    actuator = (shaft(11.2, 19.1, 2)
                .fuse(shaft(18.1, 18.6, 4.6))
                .fuse(shaft(18.6, 19.1, 2.8))
                .fuse(shaft(19.1, 20.1, 2)).fuse(tip).clean()
                .translate((-displacement, 0, 0)))
    wire_radius = 0.2
    spring_length = 18.1 - displacement - 12.2 - 2 * wire_radius
    helix = cq.Wire.makeHelix(spring_length / 4, spring_length, 1.85)
    profile = cq.Plane(origin=helix.startPoint(), normal=helix.tangentAt(0))
    spring = (cq.Workplane(profile).circle(wire_radius)
              .sweep(cq.Workplane(obj=helix), isFrenet=True).val()
              .rotate((0, 0, 0), (0, 1, 0), 90)
              .translate((12.2 + wire_radius, 0, AXIS_Z)))
    parts = [
        ("shell", shell, METAL),
        ("base", base, PLASTIC),
        ("actuator", actuator, (0.27, 0.28, 0.26)),
        ("spring", spring, (0.40, 0.42, 0.44)),
    ]
    for index, x in enumerate((3.2, 5.7, 8.2)):
        terminal = (cq.Workplane(obj=box(x - 0.25, -0.2, -2.8,
                                        x + 0.25, 0.2, 0.8))
                    .faces("<Z").edges().chamfer(0.08).val())
        parts.append((f"terminal_{index}", terminal, METAL))
    for end, x, y in (("rear", 0.4, -1.1), ("front", 11.9, 2.2)):
        tab = (cq.Workplane(obj=box(x - 0.4, y - 0.3, -3.3,
                                   x + 0.4, y + 0.3, 1.4))
               .faces("<Z").edges().chamfer(0.1).val())
        parts.append((f"mounting_tab_{end}", tab, METAL))
    return parts


def validate(parts, displacement):
    if len(parts) != 9:
        raise ValueError("Expected housing, base, actuator, spring, three pins and two tabs")
    for name, shape, _ in parts:
        if not shape.isValid() or len(shape.Solids()) != 1 or shape.Volume() <= 0:
            raise ValueError(f"Invalid solid: {name}")
    shapes = {name: shape for name, shape, _ in parts}
    shell = shapes["shell"].BoundingBox()
    check_close(shell.xlen, 12.2, "housing length")
    check_close(shell.ylen, 5, "housing width")
    check_close(shell.zmax, 8.3, "housing top")
    check_close(shapes["base"].BoundingBox().zmin, 0, "seating plane")
    actuator = shapes["actuator"]
    check_close(actuator.BoundingBox().xmax, 22.1 - displacement, "tip position")
    check_close(actuator.BoundingBox().center.z, 4.7, "actuator axis height")
    tip = actuator.intersect(box(20.1 - displacement, -3, 0,
                                 22.2 - displacement, 3, 8.3)).BoundingBox()
    for actual, expected in zip((tip.xlen, tip.ylen, tip.zlen), (2, 2.8, 2.8)):
        check_close(actual, expected, "tip dimensions")
    for index in range(3):
        bounds = shapes[f"terminal_{index}"].BoundingBox()
        check_close(bounds.xlen, 0.5, "terminal width")
        check_close(bounds.ylen, 0.4, "terminal thickness")
        check_close(bounds.center.x, 3.2 + index * 2.5, "terminal X position")
        check_close(bounds.center.y, 0, "single terminal row")
        check_close(shell.zmax - bounds.zmin, 11.1, "terminal tip height")
    for end, y in (("rear", -1.1), ("front", 2.2)):
        bounds = shapes[f"mounting_tab_{end}"].BoundingBox()
        check_close(bounds.xlen, 0.8, "tab width")
        check_close(bounds.ylen, 0.6, "tab thickness")
        check_close(bounds.center.y, y, "tab Y offset")
        check_close(shell.zmax - bounds.zmin, 11.6, "tab tip height")
    check_close(shapes["mounting_tab_front"].BoundingBox().center.x, 11.9,
                "front tab X position")
    for other in ("shell", "spring"):
        if actuator.intersect(shapes[other]).Volume() > 1e-6:
            raise ValueError(f"Actuator interferes with {other}")


def export_step(parts, state):
    assembly = cq.Assembly(name=f"{STEM}_{state}")
    for name, shape, color in parts:
        assembly.add(shape, name=name, color=cq.Color(*color))
    path = HERE / f"{STEM}_{state}.step"
    assembly.export(str(path))
    imported = cq.importers.importStep(str(path)).val()
    if not imported.isValid() or len(imported.Solids()) != 9:
        raise ValueError(f"STEP round-trip validation failed: {path}")
    bounds = imported.BoundingBox()
    check_close(bounds.xlen, 22.1 - STATES[state], "STEP overall length")
    check_close(bounds.ylen, 5, "STEP overall width")
    check_close(bounds.zlen, 11.6, "STEP height including mounting tabs")
    expected_volume = sum(shape.Volume() for _, shape, _ in parts)
    if not isclose(imported.Volume(), expected_volume, rel_tol=1e-6):
        raise ValueError(f"STEP volume changed during export: {path}")
    print(f"PASS {path.name}: 9 valid solids; "
          f"{bounds.xlen:.3f} x {bounds.ylen:.3f} x {bounds.zlen:.3f} mm")


def preview(models):
    window = vtk.vtkRenderWindow()
    window.SetOffScreenRendering(True)
    window.SetSize(1600, 1100)
    views = (
        ("released", (35, -34, 25), "RELEASED - 22.1 mm overall length"),
        ("latched", (35, -34, 25), "LATCHED - 2.5 mm lock travel / 19.6 mm long"),
        ("pressed", (35, -34, 25), "FULLY PRESSED - 3.5 mm travel / 18.6 mm long"),
        ("released", (30, -32, -25), "UNDERSIDE - three signal pins and two mounting tabs"),
    )
    for index, (state, position, title) in enumerate(views):
        renderer = vtk.vtkRenderer()
        column, row = index % 2, 1 - index // 2
        renderer.SetViewport(column / 2, row / 2, (column + 1) / 2, (row + 1) / 2)
        renderer.SetBackground(0.94, 0.95, 0.97)
        window.AddRenderer(renderer)
        for _, shape, color in models[state]:
            renderer.AddActor(actor(shape, color))
        label = vtk.vtkTextActor()
        label.SetInput("PS-12E05 / " + title + "\nApproximate exterior - dimensions in mm")
        label.SetPosition(20, 20)
        label.GetTextProperty().SetFontSize(16)
        label.GetTextProperty().SetColor(0.16, 0.18, 0.22)
        renderer.AddViewProp(label)
        camera = renderer.GetActiveCamera()
        camera.SetPosition(*position)
        camera.SetFocalPoint(10, 0, 3)
        camera.SetViewUp(0, 0, 1)
        camera.ParallelProjectionOn()
        camera.SetParallelScale(12)
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
    models = {state: build(displacement) for state, displacement in STATES.items()}
    released = {name: shape for name, shape, _ in models["released"]}
    for state, parts in models.items():
        validate(parts, STATES[state])
        for name, shape, _ in parts:
            if name != "spring":
                expected = released[name]
                if name == "actuator":
                    expected = expected.translate((-STATES[state], 0, 0))
                check_close(shape.Volume(), expected.Volume(), f"{state} {name} volume")
                check_close((shape.Center() - expected.Center()).Length, 0,
                            f"{state} {name} position")
        export_step(parts, state)
    preview(models)


if __name__ == "__main__":
    main()
