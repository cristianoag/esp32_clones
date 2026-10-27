import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SOFTWARE = Path(__file__).resolve().parents[1]
EMULATORS = ("apple2", "c64", "cp400", "macplus", "msx", "msxturbor", "sms", "tk95")


class MakefileTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="clone firmware ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        shutil.copy2(SOFTWARE / "Makefile", self.root / "Makefile")
        self.root.joinpath("tools").mkdir()
        shutil.copy2(
            SOFTWARE / "tools" / "copy_firmware.py",
            self.root / "tools" / "copy_firmware.py",
        )

    def package(self, emulator, version="1.00", payload=b"firmware"):
        project = self.root / f"esp32_{emulator}_emulator"
        project.joinpath("dist").mkdir(parents=True, exist_ok=True)
        package = project / "dist" / f"ESP32_{emulator.upper()}-{version}.FLH"
        package.write_bytes(payload)
        return package

    def seed_packages(self):
        for emulator in EMULATORS:
            self.package(emulator)

    def make(self, *targets):
        return subprocess.run(
            ["make", "--no-print-directory", f'PYTHON="{sys.executable}"', *targets],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
        )

    def test_copies_latest_version_for_all_eight_projects_without_building(self):
        self.seed_packages()
        latest = self.package("cp400", "2.00", b"latest package")
        newer_timestamp = self.package("cp400", "1.99", b"older version")
        os.utime(latest, (1_000_000_000, 1_000_000_000))
        os.utime(newer_timestamp, (1_100_000_000, 1_100_000_000))
        latest.parent.joinpath("ESP32_MSX-9.99.FLH").write_bytes(b"wrong emulator")
        latest.parent.joinpath("ESP32_CP400-latest.FLH").write_bytes(b"invalid name")
        latest.parent.joinpath("ESP32_CP400-9.99.FLH").mkdir()
        self.root.joinpath("esp32_not_a_project_emulator").write_text("not a folder")

        result = self.make("firmware")

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for emulator in EMULATORS:
            project = self.root / f"esp32_{emulator}_emulator"
            copied = list(project.glob("*.FLH"))
            self.assertEqual(len(copied), 1)
            source = (
                latest
                if emulator == "cp400"
                else project / "dist" / f"ESP32_{emulator.upper()}-1.00.FLH"
            )
            self.assertEqual(copied[0].name, source.name)
            self.assertEqual(copied[0].read_bytes(), source.read_bytes())
            self.assertEqual(
                self.root.joinpath("sdcard", source.name).read_bytes(),
                source.read_bytes(),
            )
        self.assertEqual(len(list(self.root.joinpath("sdcard").glob("*.FLH"))), 8)

    def test_replaces_same_filename_and_preserves_other_project_files(self):
        source = self.package("cp400", "1.14", b"rebuilt firmware")
        sdcard = self.root / "sdcard"
        sdcard.mkdir()
        destinations = (source.parent.parent / source.name, sdcard / source.name)
        for destination in destinations:
            destination.write_bytes(b"stale firmware")
            destination.with_name("ESP32_CP400-1.13.FLH").write_bytes(b"keep older package")
        sdcard.joinpath("keep.txt").write_text("destination only")

        for _ in range(2):
            result = self.make("firmware")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for destination in destinations:
                self.assertEqual(destination.read_bytes(), source.read_bytes())
                self.assertEqual(
                    destination.with_name("ESP32_CP400-1.13.FLH").read_bytes(),
                    b"keep older package",
                )
            self.assertEqual(sdcard.joinpath("keep.txt").read_text(), "destination only")

    def test_missing_packages_fail_before_any_firmware_is_copied(self):
        self.seed_packages()
        for emulator in ("sms", "tk95"):
            self.package(emulator).unlink()
        self.root.joinpath("esp32_tk95_emulator", "dist").rmdir()

        result = self.make("firmware")

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing firmware packages", result.stderr)
        self.assertIn("esp32_sms_emulator", result.stderr)
        self.assertIn("esp32_tk95_emulator", result.stderr)
        self.assertFalse(self.root.joinpath("sdcard").exists())
        for emulator in EMULATORS:
            self.assertEqual(
                list(self.root.joinpath(f"esp32_{emulator}_emulator").glob("*.FLH")),
                [],
            )

    def test_no_projects_is_an_error(self):
        result = self.make("firmware")

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No emulator projects", result.stderr)

    def test_default_target_also_preserves_sdcard_merge_behavior(self):
        self.seed_packages()
        local = self.root / "esp32_cp400_emulator" / "sdcard" / "cp400"
        local.mkdir(parents=True)
        local.joinpath("shared.txt").write_text("local content")
        destination = self.root / "sdcard" / "cp400"
        destination.mkdir(parents=True)
        destination.joinpath("shared.txt").write_text("stale content")
        destination.joinpath("keep.txt").write_text("destination only")

        result = self.make()

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(destination.joinpath("shared.txt").read_text(), "local content")
        self.assertEqual(destination.joinpath("keep.txt").read_text(), "destination only")
        for emulator in EMULATORS:
            project = self.root / f"esp32_{emulator}_emulator"
            copied = list(project.glob("*.FLH"))
            self.assertEqual(len(copied), 1)
            self.assertEqual(
                self.root.joinpath("sdcard", copied[0].name).read_bytes(),
                copied[0].read_bytes(),
            )
            sdcard_name = "tk" if emulator == "tk95" else emulator
            self.assertTrue(self.root.joinpath("sdcard", sdcard_name).is_dir())
        self.assertEqual(len(list(self.root.joinpath("sdcard").glob("*.FLH"))), 8)

    def test_parallel_default_target_copies_firmware_after_sdcard_merge(self):
        source = self.package("cp400", "1.14", b"latest firmware")
        local = source.parent.parent / "sdcard"
        local.mkdir()
        local.joinpath(source.name).write_bytes(b"stale local copy")

        result = self.make("-j4")

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(
            self.root.joinpath("sdcard", source.name).read_bytes(),
            source.read_bytes(),
        )

    def test_sdcard_target_does_not_require_firmware(self):
        self.root.joinpath("esp32_cp400_emulator").mkdir()

        result = self.make("sdcard")

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(self.root.joinpath("sdcard", "cp400", "bios").is_dir())
        self.assertFalse(self.root.joinpath("esp32_cp400_emulator", "dist").exists())


if __name__ == "__main__":
    unittest.main()
