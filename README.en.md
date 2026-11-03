# bectl-tui

[Português](README.md) | **English**

[![Build & Test](https://github.com/cleberalves40/bectl-tui/actions/workflows/build.yml/badge.svg)](https://github.com/cleberalves40/bectl-tui/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A terminal interface (TUI) for FreeBSD's [`bectl(8)`](https://man.freebsd.org/cgi/man.cgi?bectl(8)) utility to manage ZFS Boot Environments. It is the terminal counterpart to [bectl-gui](https://github.com/cleberalves40/bectl-gui), with the same contexts and shortcuts wherever the terminal permits, and [Nerd Fonts](https://www.nerdfonts.com/) icons.

```
 bectl — Boot Environment Manager                         Filter by name…
──────────────────────────────────────────────────────────────────────────────
    Boot Environment              Active Mount             Space  Created
    default                       NR     /                  1.2G  2026-10-01 10:00
    13.2-before-upgrade           -      -                   300M  2026-10-05 09:30
    test                          -      /mnt/be              10M  2026-10-06 11:00
──────────────────────────────────────────────────────────────────────────────
  1  Info          2  Output  3  Help   4  Snapshots (2)
 default
 State: Active now
──────────────────────────────────────────────────────────────────────────────
 Ready.                                                  Theme: System  3 BE(s)
 F1 Help  F2 Rename  F5 Refresh  F6 Sort  F9 Menu  F10 Quit
```

## Status

**v0.2.0: operations.** In addition to listing, filtering, and sorting, it creates, activates (including temporarily), renames, destroys (multiple at once, protecting the BE in use), mounts, and unmounts BEs, and creates, clones, and destroys snapshots, with the dialogs, confirmations, and menus from bectl-gui. Preferences, themes, and languages are planned for v0.3.0; v1.0.0 will reach full parity with bectl-gui 1.2.0. See the [CHANGELOG](CHANGELOG.en.md).

### Password (doas/sudo)

As a regular user, write operations use `doas` by default. Set `elevator=sudo` in `~/.config/bectl-tui/config` to use `sudo`, or set `elevator=` to run `bectl` directly. If `doas` does not prompt for a password (a `nopass` rule in `doas.conf`), the operation runs in the background. If a password is required, the screen is suspended, the terminal shows the command and password prompt, and the interface returns afterward. As root, `bectl` runs directly.

## Requirements (FreeBSD)

```sh
pkg install ftxui cmake ninja
pkg install nerd-fonts-jetbrainsmono      # icons; optional
```

If `pkg` cannot find `ftxui` (the FreeBSD 15/aarch64 `latest` branch was missing it in October 2026), install it from `quarterly`; see `ci/freebsd-ci.sh`.

For icons, configure the terminal to use **JetBrainsMono Nerd Font Mono**. The *Mono* variant keeps each icon one character wide so columns stay aligned. Without a Nerd Font, use `--icons=unicode` or `--icons=ascii`.

Unlike bectl-gui, bectl-tui does not depend on Qt: FTXUI is a single package with no X11 or Wayland dependency. It is suitable, for example, for a server accessed over SSH.

## Build

```sh
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bectl-tui
```

Options: `--icons=nerd|unicode|ascii`, `--lang=en_US|pt_BR|es_ES`, `--config=FILE`, and `--version`. Preferences are stored in `~/.config/bectl-tui/config`; currently, these are table sort order and `elevator` (doas, sudo, or empty).

## Keyboard and mouse (v0.2.0)

| Key | Action |
|:---|:---|
| <kbd>F5</kbd> / <kbd>Ctrl+R</kbd> | Refresh the list |
| <kbd>↑</kbd> <kbd>↓</kbd> <kbd>PgUp</kbd> <kbd>PgDn</kbd> <kbd>Home</kbd> <kbd>End</kbd> (or <kbd>j</kbd> <kbd>k</kbd>) | Move selection |
| <kbd>Ctrl+F</kbd> / <kbd>/</kbd> | Filter by name (<kbd>Esc</kbd> or <kbd>Enter</kbd> returns to the table) |
| <kbd>F6</kbd> | Choose the sort column: press again on the same column to reverse; choose "bectl order" to restore the original order |
| Click a header | Sort ascending, descending, or in bectl order |
| <kbd>1</kbd>-<kbd>4</kbd> / <kbd>Alt+1</kbd>-<kbd>Alt+4</kbd> | Info, Output, Help, and Snapshots tabs (<kbd>F1</kbd>: Help) |
| <kbd>Tab</kbd> | Switch focus between the BE and snapshot tables |
| <kbd>Ctrl+Q</kbd> / <kbd>F10</kbd> / <kbd>q</kbd> | Quit |
| <kbd>Ctrl+N</kbd> | Create Boot Environment |
| <kbd>Enter</kbd> / <kbd>Ctrl+T</kbd> | Activate / activate temporarily (with confirmation) |
| <kbd>F2</kbd> | Rename |
| <kbd>Space</kbd> | Mark multiple BEs for destruction |
| <kbd>Delete</kbd> | Destroy marked BEs, or the selected BE |
| <kbd>Alt+M</kbd> / <kbd>Ctrl+U</kbd> | Mount / unmount |
| <kbd>Ctrl+S</kbd> | Create snapshot |
| <kbd>Enter</kbd> / <kbd>Delete</kbd> in the snapshots table | Clone / destroy the snapshot |
| <kbd>F9</kbd>, <kbd>Alt+A</kbd>, <kbd>Alt+C</kbd>, <kbd>Alt+J</kbd> | Actions, File, Actions, and Help menus |

The full keymap, including equivalents for bectl-gui shortcuts, is in [docs/KEYMAP.en.md](docs/KEYMAP.en.md).

## Architecture

| Layer | Location | Responsibility |
|:---|:---|:---|
| Core | `src/core/` | Run `bectl(8)` without a shell; `libbe` name validation; parse `bectl list` and `list -s`; sizes, i18n, and preferences. Pure C++17: a port of bectl-gui 1.2.0's `BectlManager`. |
| Model | `src/model/` | Filtering, sorting (space in bytes, date, state), and selection that follows the BE |
| Interface | `src/tui/` | FTXUI screen, dialogs, menus, keyboard and mouse; background listing and operations; returns the terminal to doas/sudo when a password is required |

## Tests

```sh
cmake -B build -S . -DBECTL_TUI_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
sh tests/freebsd-e2e.sh build    # on FreeBSD, as root: integration with real bectl
```

- `test_core`: port of bectl-gui's `tst_validation` (same cases), plus process execution and preferences.
- `test_model`: filtering, sorting, and selection.
- `test_view`: renders the screen as text and drives the interface with keyboard events, including each operation and dialog against a fake `bectl` that records commands.
- `test_integration`: port of `tst_bectlmanager`, against real `bectl(8)` in a scratch ZFS pool without touching system BEs.

CI runs on Ubuntu (with ASan and UBSan) and FreeBSD 14.5 and 15.1, on amd64 and aarch64. It also runs the interface in a pseudo-terminal: on FreeBSD, it creates and destroys a real BE in the scratch pool through the dialogs.

## License

[MIT](LICENSE).
