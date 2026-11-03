# Changelog

[Português](CHANGELOG.md) | **English**

All notable changes to bectl-tui are recorded here. This changelog follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project follows [Semantic Versioning](https://semver.org/) with `vX.Y.Z` tags.

## [0.2.0] - 2026-10-19

Operations and dialogs.

### Added
- Operations from bectl-gui 1.2.0, with the same text, validation, and confirmations: create (**Ctrl+N**: name, origin - current BE, another BE, or snapshot - and `-r`); activate (**Enter**) and activate temporarily (**Ctrl+T**); rename (**F2**, with selection following the new name); destroy (**Delete**); mount (**Alt+M**, unmounted BEs only) and unmount (**Ctrl+U**, never the BE in use); create snapshot (**Ctrl+S**); and clone (**Enter**) and destroy (**Delete**) snapshots when the snapshots table is focused.
- Press **Space** to mark multiple BEs for destruction. The BE in use is skipped and listed in the confirmation ("Skipped (active)"); the result shows destroyed BEs and failures.
- Form, confirmation (destruction confirmation is red and initially focuses **No**), and message dialogs, layered over the screen.
- File, Actions, and Help menus (**F9**, **Alt+A**, **Alt+C**, **Alt+J**), with each item's shortcuts; Help > About shows the version.
- mc-style function-key bar (F1, F2, F5, F6, F9, F10) and a Help tab listing all shortcuts.
- doas/sudo support: operations run in the background when no password is required; if doas/sudo requests a password, the screen is suspended while it uses the terminal. The `elevator` preference is stored in the config file.
- `bectl` errors appear in a dialog, the status area, and the Output tab.
- Tests: each operation and dialog driven by keyboard against a fake `bectl`; on FreeBSD, the interface creates and destroys a real BE in a scratch pool through a pseudo-terminal; smoke test opens the create dialog.

## [0.1.0] - 2026-10-19

First release: read-only.

### Added
- Main FTXUI screen following bectl-gui's structure: Boot Environment table (state, name, active, mount, space, created), filter field, **Info**, **Output**, **Help**, and **Snapshots** tabs, status bar, and function-key bar.
- Nerd Font icons (default), Unicode, or ASCII (`--icons`).
- Column sorting matching bectl-gui 1.2.0: space in bytes, chronological date, and state (in use, next boot, inactive); click a header (ascending, descending, `bectl` order) or press **F6**. The order is saved in `~/.config/bectl-tui/config`.
- Selection follows the BE while filtering, sorting, and refreshing.
- Background listing: the screen remains responsive while `bectl` runs.
- Shortcuts: F5/Ctrl+R, Ctrl+F and /, F6, 1-4 and Alt+1-4, F1, Tab, Ctrl+Q, F10, and q; mouse support for headers, rows, and tabs (see `docs/KEYMAP.en.md`).
- en_US, pt_BR, and es_ES languages, with bectl-gui's text (`--lang`).
- Dependency-free C++17 core: port of bectl-gui 1.2.0's `BectlManager`, with the same validation rules and parsing of `bectl list` and `bectl list -s`.
- Tests: core (port of `tst_validation`), model, view, and integration against real `bectl(8)` (port of `tst_bectlmanager`), plus the interface in a pseudo-terminal (`tests/tui-smoke.sh` and `tests/freebsd-e2e.sh`); CI on Ubuntu (FTXUI 6.1.9, ASan, and UBSan) and FreeBSD 14.5 and 15.1, amd64 and aarch64.

[0.2.0]: https://github.com/cleberalves40/bectl-tui/releases/tag/v0.2.0
[0.1.0]: https://github.com/cleberalves40/bectl-tui/releases/tag/v0.1.0
