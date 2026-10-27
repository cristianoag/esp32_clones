import re
from pathlib import Path
from shutil import copy2


def copy_firmware(root: Path) -> None:
    projects = sorted(path for path in root.glob("esp32_*_emulator") if path.is_dir())
    if not projects:
        raise SystemExit(f"No emulator projects found in {root}.")

    packages = []
    missing = []
    for project in projects:
        prefix = project.name.removesuffix("_emulator").upper()
        pattern = re.compile(rf"{re.escape(prefix)}-(\d)\.(\d{{2}})\.FLH")
        candidates = []
        for path in project.joinpath("dist").glob("*.FLH"):
            match = pattern.fullmatch(path.name)
            if path.is_file() and match:
                version = tuple(map(int, match.groups()))
                candidates.append((version, path))
        if not candidates:
            missing.append(str(project / "dist"))
            continue
        source = max(candidates)[1]
        packages.append((source, project / source.name))

    if missing:
        raise SystemExit(
            "Missing firmware packages in: "
            + ", ".join(missing)
            + ". Run make firmware in each affected emulator project first."
        )

    sdcard = root / "sdcard"
    sdcard.mkdir(parents=True, exist_ok=True)
    for source, project_destination in packages:
        for destination in (project_destination, sdcard / source.name):
            copy2(source, destination)
            print(f"Copied {source} to {destination}")


if __name__ == "__main__":
    copy_firmware(Path(__file__).resolve().parents[1])
