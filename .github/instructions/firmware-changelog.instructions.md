---
description: "Maintain the ESP32 Clone Series firmware change logs whenever firmware behavior, build output, configuration, or user-facing features change."
applyTo: "software/**"
---

# Firmware Change Logs

Each firmware lives in its own folder under `software/`, and each folder keeps its
own change log at `software/<firmware>/docs/log.md`. The logs are independent:
they have separate version numbers and separate release histories.

`software/esp32_cp400_emulator/docs/log.md` is the reference for the format.

## Which log to update

For every incorporated firmware change, update the log of the firmware that owns
the changed files, in the same change set.

- Change a file under `software/<firmware>/`, update `software/<firmware>/docs/log.md`.
- Never record a change in another firmware's log, and never merge several
  firmwares into one shared log.
- When one change set touches more than one firmware, update each affected log
  separately, describing only that firmware's part of the change.
- If the folder has no `docs/log.md` yet, create it, including the `docs` folder,
  using the structure below.

## Version numbers

- Use the version defined by `FW_VERSION` in that same firmware's
  `software/<firmware>/Makefile`. Never take the version from another firmware.
- Format it as one major digit and two minor digits, for example `1.10`.
- Add entries to the existing section when it already matches `FW_VERSION`.
- Create a new version section when `FW_VERSION` changes, and do not rewrite
  previously released sections.
- Include the release date as `YYYY-MM-DD` when it is known. Use `Unreleased`
  when the work targets a version that has not been released yet.

## File structure

Every log starts with an `# Firmware Change Log` heading, one or two lines naming
the firmware the log belongs to, and a note on the version format. Version
sections follow in reverse chronological order, newest first:

```markdown
# Firmware Change Log

This log tracks user-visible changes to the <firmware name> firmware.
Versions use one major digit and two minor digits, for example 1.10 and 1.11.

## 1.13 - Unreleased

### Fixed

- <entry>

## 1.12 - 2026-08-29

### Changed

- <entry>
```

- Version headings are `## <version> - <date or Unreleased>`.
- Group entries under `### Added`, `### Changed`, `### Fixed`, or `### Removed`.
  Use `### Notes` only for information that is not itself a change, such as a
  required rewiring or a known limitation.
- Include only the groups that have entries, and do not repeat a group within a
  version section.

## Writing entries

- Write each entry as a complete sentence in the past tense, describing the
  change from the user's point of view.
- Say what changed and, when it is not obvious, why it changed or what was wrong
  before. Prefer the observable symptom over the internal cause.
- Name user-facing things as the user sees them: menu entries, key names, GPIO
  numbers, and `make` targets.
- Keep entries self-contained. Do not refer to commits, pull requests, issue
  numbers, or source symbols and file names.

## What not to log

Do not add entries for formatting-only edits, or for internal refactors with no
firmware, build, configuration, or user-visible effect.