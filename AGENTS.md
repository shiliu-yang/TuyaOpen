# AGENTS.md

## Cursor Cloud specific instructions

### Overview

TuyaOpen is a cross-platform IoT (C/C++) SDK for building AI-agent-powered smart hardware. It targets multiple hardware platforms (Tuya T-series MCUs, ESP32, Raspberry Pi, Linux/Ubuntu). The LINUX platform target can be compiled and run natively on the development host.

### Environment setup

环境配置通过在仓库根目录下执行 export 脚本完成，脚本会创建 Python venv（`.venv/`）、安装 pip 依赖（`requirements.txt`）、设置 `OPEN_SDK_ROOT`/`OPEN_SDK_PYTHON`/`OPEN_SDK_PIP` 环境变量。根据宿主机操作系统选择对应脚本：

- **Linux / macOS**：在 bash 下执行 `source export.sh`
- **Windows**：执行 `export.bat` 或 `export.ps1`

Cursor Cloud 环境为 Linux，因此使用：

```bash
cd /workspace && . ./export.sh
```

如果 venv 已存在，脚本会跳过创建步骤直接激活。执行完成后即可使用 `tos.py` 系列命令。

### Build workflow

Standard commands (see README.md):
1. `tos.py check` — verifies tools (git, cmake, make, ninja) and downloads git submodules
2. `cd examples/<category>/<project>` then `tos.py build` — builds for configured platform
3. For LINUX target: the output is a native ELF binary in `dist/`

To build for LINUX/Ubuntu (runnable on this host), the project's `app_default.config` must contain:
```
CONFIG_BOARD_CHOICE_LINUX=y
CONFIG_BOARD_CHOICE_UBUNTU=y
```

The default config for `examples/get-started/sample_project` targets T5AI. Change it to LINUX before building, then restore if needed.

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
