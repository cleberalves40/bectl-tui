#ifndef BECTL_TUI_ICONS_H
#define BECTL_TUI_ICONS_H

#include <string>

namespace tui {

/**
 * Ícones da interface. "nerd" usa glifos Nerd Fonts (Font Awesome, U+Fxxx),
 * que exigem uma fonte Nerd no terminal, como a JetBrainsMono Nerd Font Mono
 * (pkg install nerd-fonts-jetbrainsmono); "unicode" e "ascii" servem para
 * terminais sem ela.
 */
struct IconSet {
    const char *activeNow;
    const char *activeReboot;
    const char *temporary;
    const char *inactive;
    const char *mounted;
    const char *snapshot;
    const char *info;
    const char *output;
    const char *help;
    const char *filter;
    const char *sortAscending;
    const char *sortDescending;
    const char *busy;
    const char *error;
};

enum class IconMode { Nerd, Unicode, Ascii };

const IconSet &icons(IconMode mode);
IconMode iconModeFromString(const std::string &s);   // "nerd" (padrão), "unicode", "ascii"
const char *iconModeName(IconMode mode);

} // namespace tui

#endif
