# AGENTS.md

## Cursor Cloud specific instructions

### Overview

TuyaOpen is a cross-platform IoT (C/C++) SDK for building AI-agent-powered smart hardware. It targets multiple hardware platforms (Tuya T-series MCUs, ESP32, Raspberry Pi, Linux/Ubuntu). The LINUX platform target can be compiled and run natively on the development host.

### Environment setup

Run the export script from the repo root to set up the environment. It creates a Python venv (`.venv/`), installs pip dependencies (`requirements.txt`), and sets `OPEN_SDK_ROOT`/`OPEN_SDK_PYTHON`/`OPEN_SDK_PIP` environment variables. Choose the script matching the host OS:

- **Linux / macOS**: run `. ./export.sh` in bash
- **Windows**: run `export.bat` or `export.ps1`

Cursor Cloud runs Linux, so use:

```bash
cd /workspace && . ./export.sh
```

If the venv already exists, the script skips creation and activates it directly. After execution, `tos.py` commands are available.

### Build workflow

Standard commands (see README.md):
1. `tos.py check` — verifies tools (git, cmake, make, ninja) and downloads git submodules
2. `cd examples/<category>/<project>` then `tos.py build` — builds for configured platform
3. For LINUX target: the output is a native ELF binary in `dist/`

To build for LINUX/Ubuntu (runnable on this host), use `tos.py config choice` in the project directory to select the LINUX/Ubuntu platform. This is an interactive command that presents a menu for platform selection.

### Lint / format

- `python tools/check_format.py --debug --files <file>` or `--dir <dir>` — checks C/C++ format via clang-format, Chinese character detection, and file header validation
- `python tools/check_format.py --base <branch>` — PR mode, checks files changed relative to base branch
- Requires `clang-format` (installed as system package)

### System dependencies

The following system packages are needed (see `Dockerfile`):
`build-essential`, `libsystemd-dev`, `locales`, `libc6-i386`, `libusb-1.0-0`, `libusb-1.0-0-dev`, `python3`, `python3-pip`, `python3-venv`, `clang-format`

### Gotchas

- `tos.py` config commands (`config choice`, `config menu`) are interactive (use TTY menus). Avoid them in non-interactive shells. Instead, directly write `app_default.config` with the desired Kconfig values.
- The platform SDK (e.g. `TuyaOpen-ubuntu`) is fetched on first build via `tos.py build` or `tos.py update` from GitHub. It is cached at `platform/LINUX/`.
- The `check_platform_commit` function in the build flow may prompt for input if the platform commit doesn't match. Create `.cache/.dont_prompt_update_platform` to suppress this.
- The build creates artifacts in `<project>/.build/` and `<project>/dist/`.
