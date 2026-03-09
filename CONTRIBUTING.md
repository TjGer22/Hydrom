# Contributing to Hydrom

Thank you for your interest in contributing to Hydrom! This document explains how to get started, what kinds of contributions are welcome, and how the project is structured.

---

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Ways to Contribute](#ways-to-contribute)
- [Development Setup](#development-setup)
- [Branching Strategy](#branching-strategy)
- [Commit Messages](#commit-messages)
- [Pull Request Process](#pull-request-process)
- [Reporting Bugs](#reporting-bugs)
- [Hardware Contributions](#hardware-contributions)
- [Documentation Contributions](#documentation-contributions)

---

## Code of Conduct

By participating in this project, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md). Please report unacceptable behaviour to the maintainers.

---

## Ways to Contribute

- **Bug reports** — open an issue using the bug-report template
- **Feature requests** — open an issue using the feature-request template
- **Code / firmware** — fix bugs, add features, improve test coverage
- **Hardware** — improvements to the PCB design, component substitutions, enclosure designs
- **Documentation** — fix typos, improve explanations, add screenshots or GIFs
- **Translations** — improve or add documentation in other languages

---

## Development Setup

### Firmware

1. Install [PlatformIO](https://platformio.org/) (VS Code extension recommended).
2. Clone the repository:
   ```bash
   git clone https://github.com/TjGer22/Hydrom.git
   cd hydrom/firmware
   ```
3. Copy `lib/FileManager/Credentials.txt.example` to `lib/FileManager/Credentials.txt` and fill in your own values (this file is in `.gitignore` and will never be committed).
4. Build and upload:
   ```bash
   pio run --environment local --target upload
   ```
5. Open the serial monitor:
   ```bash
   pio device monitor
   ```

### Documentation

The docs use [MkDocs](https://www.mkdocs.org/) with the [Material theme](https://squidfunk.github.io/mkdocs-material/).

```bash
pip install mkdocs-material
cd hydrom
mkdocs serve        # live preview at http://localhost:8000
mkdocs build        # build static site into site/
```

### Hardware

PCB files are in `hardware/pcb/` and require [KiCad](https://www.kicad.org/) 6 or later.

---

## Branching Strategy

| Branch | Purpose |
|---|---|
| `main` | Stable, release-ready code |
| `develop` | Integration branch for new features |
| `feature/<name>` | New features or improvements |
| `fix/<name>` | Bug fixes |
| `docs/<name>` | Documentation-only changes |
| `hw/<name>` | Hardware design changes |

Please branch off `develop` for features and fixes, and open pull requests back to `develop`. Releases are merged from `develop` into `main`.

---

## Commit Messages

We follow the [Conventional Commits](https://www.conventionalcommits.org/) specification:

```
<type>(<scope>): <short summary>

[optional body]

[optional footer]
```

**Types:** `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `hw`

Examples:

```
feat(sensors): add DS18B20 temperature offset compensation
fix(network): retry Wi-Fi connection on DNS failure
docs(calibration): add reference-method screenshots
hw(pcb): replace LDO with lower-dropout alternative
```

---

## Pull Request Process

1. Make sure `pio run` builds cleanly and `pio check` reports no high-severity issues.
2. Update `CHANGELOG.md` under the `[Unreleased]` section.
3. Update the documentation in `docs/` if your change affects user-facing behaviour.
4. Open a pull request against `develop` and fill in the PR template.
5. At least one maintainer must approve the PR before it can be merged.

---

## Reporting Bugs

Please use the [bug-report issue template](.github/ISSUE_TEMPLATE/bug_report.md). Include:

- Firmware version (shown on the Web UI or in the build log)
- Hardware revision (printed on the PCB silkscreen)
- Steps to reproduce
- Expected vs. actual behaviour
- Serial monitor output (if available)

---

## Hardware Contributions

Hardware changes follow the same branch and PR workflow. Please also:

- Run DRC (Design Rule Check) in KiCad before submitting.
- Export updated Gerbers, BOM, and centroid files to `hardware/production/`.
- Add a brief description of the change and the reason to the PR body.

---

## Documentation Contributions

Documentation lives in `docs/` and is written in Markdown. Image assets go into `docs/assets/images/`. Please keep all documentation in **English**.

---

Thank you for helping make Hydrom better!
