#include "icons.h"

namespace tui {

namespace {

// Nerd Fonts 3.x, faixa Font Awesome (nomes nf-fa-*).
const IconSet kNerd = {
    "",   // check_circle
    "",   // repeat
    "",   // clock_o
    "",   // circle_o
    "",   // hdd_o
    "",   // camera
    "",   // info_circle
    "",   // terminal
    "",   // question_circle
    "",   // filter
    "",   // sort_asc
    "",   // sort_desc
    "",   // spinner
    "",   // exclamation_triangle
};

const IconSet kUnicode = {"●", "↻", "◷", "○", "▣", "◆", "ℹ", "»", "?", "⌕", "▲", "▼", "…", "!"};

const IconSet kAscii = {"N", "R", "T", "-", "M", "@", "i", ">", "?", "/", "^", "v", "*", "!"};

} // namespace

const IconSet &icons(IconMode mode)
{
    switch (mode) {
    case IconMode::Unicode: return kUnicode;
    case IconMode::Ascii:   return kAscii;
    default:                return kNerd;
    }
}

IconMode iconModeFromString(const std::string &s)
{
    if (s == "unicode")
        return IconMode::Unicode;
    if (s == "ascii")
        return IconMode::Ascii;
    return IconMode::Nerd;
}

const char *iconModeName(IconMode mode)
{
    switch (mode) {
    case IconMode::Unicode: return "unicode";
    case IconMode::Ascii:   return "ascii";
    default:                return "nerd";
    }
}

} // namespace tui
