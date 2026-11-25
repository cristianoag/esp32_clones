"""Approximate exterior model reconstructed from the user's dimensioned drawing.

Requires cadquery==2.8.0 (including its VTK dependency).
Run this file to regenerate both STEP assemblies and the PNG preview beside it.
No PCB footprint or model assignment is changed.

Units: mm. Origin: rear of housing, width center, nominal PCB seating plane.
+X points toward the actuator; +Z points away from the PCB.

Drawing dimensions used:
  Housing length/width/height: 12 / 6.7 / 8.3.
  Released actuator projection: 10; full travel: 2.5.
  Actuator axis above seating plane: 4.7 (interpreted from the end view).
  Tip: 2 long, 2.8 square; neck: 1 long, 2 square.
  Flange rear face: 4 behind the released tip.
  Signal terminals: 0.5 x 0.4, 2.5 pitch in both directions.
  Signal terminal tip to housing top: 12.
  Mounting-tab tip to housing top: 11.1; longitudinal tab pitch: 11.4.
  Front mounting-tab center to nearest signal-terminal center: 3.2.

Assumptions (NOT manufacturing specifications):
  Six signal terminals in two rows of three; four mounting tabs.
  Main shaft 2.5 square; flange 4.6 square x 0.4 thick.
  Metal shell 0.35 thick, cosmetic lower-edge reliefs, plastic base 1.2 thick.
  Mounting tabs 0.6 x 0.6 with centers at Y=+/-2.45.
  The end-view 1.2 callout is treated as terminal-row to mounting-tab
  center spacing; confirm this against the physical part before footprint use.
  Spring mean radius 2.05, wire diameter 0.32, four turns; idealized open ends.
  Corner radii, colors and internal overlap are cosmetic. No contacts, latch,
  electrical pin numbering, tolerances or operating-force model are supplied.
  Pressed state is full mechanical travel, not an inferred latched position.

Use for visualization and preliminary clearance checks, not tooling or
production footprint validation. Verify against the actual switch.
"""

from math import isclose
from pathlib import Path

import cadquery as cq
import vtk


HERE = Path(__file__).resolve().parent
STEM = "pushbutton_12x6_7"
AXIS_Z = 4.7
TRAVEL = 2.5
METAL = (0.67, 0.69, 0.72)
PLASTIC = (0.14, 0.15, 0.17)
ACTUATOR = (0.88, 0.85, 0.74)


def box(x0, y0, z0, x1, y1, z1):
    return cq.Solid.makeBox(x1 - x0, y1 - y0, z1 - z0, cq.Vector(x0, y0, z0))


def square_shaft(x0, x1, size):
    return box(x0, -size / 2, AXIS_Z - size / 2,
               x1, size / 2, AXIS_Z + size / 2)


def build(pressed=False):
    displacement = TRAVEL if pressed else 0.0
    outer = (cq.Workplane("XY").center(6, 0).rect(12, 6.7).extrude(7.5)
             .edges("|Z").fillet(0.45).translate((0, 0, 0.8)).val())
    shell = outer.cut(box(0.35, -3, 0.7, 11.65, 3, 7.95))
    shell = shell.cut(square_shaft(11.5, 12.1, 3.0))
    for y0, y1 in ((-3.5, -2.9), (2.9, 3.5)):
        for x0, x1 in ((1.4, 2.2), (4.2, 7.8), (9.8, 10.6)):
            shell = shell.cut(box(x0, y0, 0.7, x1, y1, 1.6))
    base = (cq.Workplane("XY").center(6, 0).rect(11.3, 5.9).extrude(1.2)
            .edges("|Z").fillet(0.25).val())
    tip = (cq.Workplane(obj=square_shaft(20, 22, 2.8))
           .faces(">X").edges().fillet(0.25).val())
    actuator = (square_shaft(11, 19, 2.5)
                .fuse(square_shaft(18, 18.4, 4.6))
                .fuse(square_shaft(19, 20, 2.0))
                .fuse(tip).clean().translate((-displacement, 0, 0)))
    wire_radius = 0.16
    spring_length = 6 - displacement - 2 * wire_radius
    helix = cq.Wire.makeHelix(spring_length / 4, spring_length, 2.05)
    profile = cq.Plane(origin=helix.startPoint(), normal=helix.tangentAt(0))
    spring = (cq.Workplane(profile).circle(wire_radius)
              .sweep(cq.Workplane(obj=helix), isFrenet=True).val()
              .rotate((0, 0, 0), (0, 1, 0), 90)
              .translate((12 + wire_radius, 0, AXIS_Z)))
    parts = [
        ("shell", shell, METAL),
        ("base", base, PLASTIC),
        ("actuator", actuator, ACTUATOR),
        ("spring", spring, (0.42, 0.44, 0.47)),
    ]
    for column, x in enumerate((3.5, 6.0, 8.5)):
        for row, y in enumerate((-1.25, 1.25)):
            pin = (cq.Workplane(obj=box(x - 0.25, y - 0.2, -3.7,
                                       x + 0.25, y + 0.2, 0.8))
                   .faces("<Z").edges().chamfer(0.08).val())
            parts.append((f"terminal_{column}_{row}", pin, METAL))
    for end, x in enumerate((0.3, 11.7)):
        for side, y in enumerate((-2.45, 2.45)):
            tab = (cq.Workplane(obj=box(x - 0.3, y - 0.3, -2.8,
                                       x + 0.3, y + 0.3, 1.4))
                   .faces("<Z").edges().chamfer(0.12).val())
            parts.append((f"mounting_tab_{end}_{side}", tab, METAL))
    return parts


def check_close(actual, expected, label):
    if not isclose(actual, expected, abs_tol=1e-5):
        raise ValueError(f"{label}: expected {expected}, got {actual}")


def validate(parts, pressed):
    for name, shape, _ in parts:
        if not shape.isValid() or len(shape.Solids()) != 1 or shape.Volume() <= 0:
            raise ValueError(f"Invalid solid: {name}")
    shapes = {name: shape for name, shape, _ in parts}
    body = shapes["shell"].BoundingBox()
    check_close(body.xlen, 12, "body length")
    check_close(body.ylen, 6.7, "body width")
    check_close(body.zmax, 8.3, "body top")
    check_close(shapes["base"].BoundingBox().zmin, 0, "seating plane")
    check_close(shapes["actuator"].BoundingBox().xmax,
                22 - (TRAVEL if pressed else 0), "actuator endpoint")
    for name, shape in shapes.items():
        if name.startswith("terminal_"):
            bounds = shape.BoundingBox()
            check_close(bounds.zmin, -3.7, f"{name} bottom")
            check_close(bounds.xlen, 0.5, f"{name} width")
            check_close(bounds.ylen, 0.4, f"{name} thickness")
        elif name.startswith("mounting_tab_"):
            check_close(shape.BoundingBox().zmin, -2.8, f"{name} bottom")
    for column in range(3):
        for row in range(2):
            center = shapes[f"terminal_{column}_{row}"].BoundingBox().center
            check_close(center.x, 3.5 + column * 2.5, "terminal X position")
            check_close(center.y, -1.25 + row * 2.5, "terminal Y position")


def export_step(parts, pressed):
    state = "pressed" if pressed else "released"
    assembly = cq.Assembly(name=f"{STEM}_{state}")
    for name, shape, color in parts:
        assembly.add(shape, name=name, color=cq.Color(*color))
    path = HERE / f"{STEM}_{state}.step"
    assembly.export(str(path))
    imported = cq.importers.importStep(str(path)).val()
    if not imported.isValid() or len(imported.Solids()) != len(parts):
        raise ValueError(f"STEP round-trip validation failed: {path}")
    bounds = imported.BoundingBox()
    check_close(bounds.xlen, 19.5 if pressed else 22, "STEP overall length")
    check_close(bounds.ylen, 6.7, "STEP overall width")
    check_close(bounds.zlen, 12, "STEP overall height including pins")
    expected_volume = sum(shape.Volume() for _, shape, _ in parts)
    if not isclose(imported.Volume(), expected_volume, rel_tol=1e-6):
        raise ValueError(f"STEP volume changed during export: {path}")
    print(f"PASS {path.name}: {len(parts)} valid solids; "
          f"{bounds.xlen:.3f} x {bounds.ylen:.3f} x {bounds.zlen:.3f} mm")


def actor(shape, color):
    vertices, triangles = shape.tessellate(0.035, 0.12)
    points = vtk.vtkPoints()
    for vertex in vertices:
        points.InsertNextPoint(vertex.x, vertex.y, vertex.z)
    cells = vtk.vtkCellArray()
    for triangle in triangles:
        cells.InsertNextCell(3, triangle)
    mesh = vtk.vtkPolyData()
    mesh.SetPoints(points)
    mesh.SetPolys(cells)
    mapper = vtk.vtkPolyDataMapper()
    mapper.SetInputData(mesh)
    item = vtk.vtkActor()
    item.SetMapper(mapper)
    item.GetProperty().SetColor(*color)
    item.GetProperty().SetSpecular(0.25)
    item.GetProperty().SetSpecularPower(25)
    return item


def preview(released, pressed):
    window = vtk.vtkRenderWindow()
    window.SetOffScreenRendering(True)
    window.SetSize(1600, 1100)
    views = (
        (released, (36, -36, 27), (0, 0, 1), "RELEASED - 22 mm overall length"),
        (pressed, (36, -36, 27), (0, 0, 1), "PRESSED - 2.5 mm travel"),
        (released, (26, -32, -24), (0, 0, 1), "UNDERSIDE - assumed six-pin / four-tab layout"),
        (released, (10, -45, 3), (0, 0, 1), "SIDE - 8.3 mm body / 12 mm including pins"),
    )
    for index, (parts, position, up, title) in enumerate(views):
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
        label.GetTextProperty().SetFontSize(18)
        label.GetTextProperty().SetColor(0.16, 0.18, 0.22)
        renderer.AddViewProp(label)
        camera = renderer.GetActiveCamera()
        camera.SetPosition(*position)
        camera.SetFocalPoint(10, 0, 3)
        camera.SetViewUp(*up)
        camera.ParallelProjectionOn()
        camera.SetParallelScale(11.5)
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
    released, pressed = build(), build(pressed=True)
    for parts, state in ((released, False), (pressed, True)):
        validate(parts, state)
        export_step(parts, state)
    preview(released, pressed)


if __name__ == "__main__":
    main()
