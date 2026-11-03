# Shortcuts: bectl-gui to bectl-tui

[Português](KEYMAP.md) | **English**

bectl-tui keeps bectl-gui 1.2.0 shortcuts whenever the terminal passes them through. Some keystrokes are not transmitted, or arrive indistinguishable from another key, so alternatives are provided:

- The terminal sends <kbd>Ctrl+M</kbd> as <kbd>Enter</kbd> and <kbd>Ctrl+I</kbd> as <kbd>Tab</kbd>.
- <kbd>Ctrl+Shift+letter</kbd> arrives the same as <kbd>Ctrl+letter</kbd>.
- Most terminals do not transmit <kbd>Ctrl+Enter</kbd>, <kbd>Ctrl+1</kbd>...<kbd>Ctrl+4</kbd>, or <kbd>Ctrl+,</kbd>.

| Action | bectl-gui | bectl-tui | Available |
|:---|:---|:---|:---|
| Refresh | F5 / Ctrl+R | **F5 / Ctrl+R** | v0.1.0 |
| Filter | Ctrl+F | **Ctrl+F** or **/** | v0.1.0 |
| Sort | Click a header | Click a header or **F6** (choose a column, as in htop) | v0.1.0 |
| Info / Output / Help / Snapshots tabs | Ctrl+1 ... Ctrl+4 | **1 ... 4** or **Alt+1 ... Alt+4**; **F1** = Help | v0.1.0 |
| Switch focused table | - | **Tab** | v0.1.0 |
| Quit | Ctrl+Q | **Ctrl+Q**, **F10**, or **q** | v0.1.0 |
| Create | Ctrl+N | **Ctrl+N** | v0.2.0 |
| Activate | Ctrl+Enter | **Enter** (with confirmation) | v0.2.0 |
| Clone snapshot | Actions menu | **Enter** with the snapshots table focused | v0.2.0 |
| Activate temporarily | Ctrl+T | **Ctrl+T** | v0.2.0 |
| Rename | F2 | **F2** | v0.2.0 |
| Create snapshot | Ctrl+Shift+S | **Ctrl+S** | v0.2.0 |
| Mount | Ctrl+M | **Alt+M** | v0.2.0 |
| Unmount | Ctrl+U | **Ctrl+U** | v0.2.0 |
| Destroy | Delete | **Delete**; with snapshots table focused, destroys the snapshot | v0.2.0 |
| Select multiple (for destruction) | Ctrl+click / Shift+click | **Space** to toggle selection | v0.2.0 |
| Menus | Menu bar | **F9** (Actions) or **Alt+A** (File), **Alt+C** (Actions), **Alt+J** (Help); left/right arrows switch menus | v0.2.0 |
| Edit menu | Menu bar | **Alt+E** | v0.3.0 |
| Preferences | Ctrl+, | **Alt+,** | v0.3.0 |
| Next theme | Ctrl+Shift+T | **Alt+T** | v0.3.0 |

FTXUI puts the terminal in raw mode, so <kbd>Ctrl+S</kbd> and <kbd>Ctrl+Q</kbd> reach the program instead of pausing or resuming the terminal (XON/XOFF flow control).

## Dialogs

| Dialog | Keys |
|:---|:---|
| Form (create, rename, mount, snapshot, clone) | **Tab**/**↓** and **Shift+Tab**/**↑** move between fields; **Space** toggles a checkbox; **←**/**→** choose an option; **Enter** confirms; **Esc** cancels |
| Confirmation | **←**/**→**/**Tab** switch buttons; **y**/**s** = Yes, **n** = No; **Enter** activates the focused button; **Esc** cancels. Destruction initially focuses **No**, as in bectl-gui. |
| Message | **Enter** or **Esc** closes |
| Menu | **←**/**→** switch menus, **↑**/**↓** choose an item, **Enter** activates it, **Esc**/**F9** closes |
