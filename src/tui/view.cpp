#include "view.h"
#include "i18n.h"

using namespace ftxui;

namespace tui {

namespace {

// Larguras fixas das colunas; o nome ocupa o restante.
constexpr int kIconWidth = 3;
constexpr int kActiveWidth = 7;
constexpr int kMountWidth = 18;
constexpr int kSpaceWidth = 8;
constexpr int kCreatedWidth = 17;

Element cell(Element e, int width)
{
    return e | size(WIDTH, EQUAL, width);
}

Element maybeReflect(Element e, Box *box)
{
    return box ? e | reflect(*box) : e;
}

std::string sortMark(const ViewState &s, int column)
{
    if (s.sort.column != column)
        return std::string();
    const IconSet &ic = icons(s.iconMode);
    return std::string(" ") + (s.sort.ascending ? ic.sortAscending : ic.sortDescending);
}

Element header(const ViewState &s, HitBoxes *boxes)
{
    const auto title = [&](int col) { return text(columnTitle(col) + sortMark(s, col)); };
    Box *b = boxes ? boxes->header.data() : nullptr;
    return hbox({
               maybeReflect(cell(text(sortMark(s, model::StateIcon).empty() ? " " : sortMark(s, model::StateIcon)),
                                 kIconWidth), b ? &b[model::StateIcon] : nullptr),
               maybeReflect(title(model::Name) | flex, b ? &b[model::Name] : nullptr),
               maybeReflect(cell(title(model::Active), kActiveWidth), b ? &b[model::Active] : nullptr),
               maybeReflect(cell(title(model::Mountpoint), kMountWidth), b ? &b[model::Mountpoint] : nullptr),
               maybeReflect(cell(title(model::Space) | align_right, kSpaceWidth), b ? &b[model::Space] : nullptr),
               text(" "),
               maybeReflect(cell(title(model::Created), kCreatedWidth), b ? &b[model::Created] : nullptr),
           })
        | bold;
}

const char *stateIcon(const ViewState &s, const bectl::BootEnvironment &be)
{
    const IconSet &ic = icons(s.iconMode);
    if (be.isActiveNow())
        return ic.activeNow;
    if (be.isTemporary())
        return ic.temporary;
    if (be.isActiveReboot())
        return ic.activeReboot;
    return ic.inactive;
}

Element row(const ViewState &s, size_t i, HitBoxes *boxes)
{
    const bectl::BootEnvironment &be = s.rows[i];
    Element name = text(be.name);
    if (be.isActiveNow())
        name = name | color(Color::Green) | bold;
    else if (be.isActiveReboot())
        name = name | color(Color::Yellow);
    Element mount = text(be.isMounted() ? std::string(icons(s.iconMode).mounted) + " " + be.mountpoint
                                        : be.mountpoint);
    const bool marked = s.marked.count(be.name) > 0;
    Element line = hbox({
        cell(hbox({text(marked ? "*" : " ") | color(Color::Cyan) | bold, text(stateIcon(s, be))}), kIconWidth),
        name | flex,
        cell(text(be.active), kActiveWidth),
        cell(mount, kMountWidth),
        cell(text(be.space) | align_right, kSpaceWidth),
        text(" "),
        cell(text(be.created), kCreatedWidth),
    });
    if (static_cast<int>(i) == s.selected) {
        line = line | (s.snapshotsFocused ? dim : inverted);
        line = line | focus;
    }
    if (boxes)
        line = line | reflect(boxes->rows[i]);
    return line;
}

Element table(const ViewState &s, HitBoxes *boxes)
{
    if (boxes)
        boxes->rows.assign(s.rows.size(), Box{});
    Elements lines;
    for (size_t i = 0; i < s.rows.size(); ++i)
        lines.push_back(row(s, i, boxes));
    if (lines.empty())
        lines.push_back(text(s.busy ? std::string() : i18n::tr("No Boot Environments.")) | dim);
    return vbox({header(s, boxes), separatorLight(), vbox(std::move(lines)) | yframe | flex});
}

Element tabs(const ViewState &s, HitBoxes *boxes)
{
    const IconSet &ic = icons(s.iconMode);
    const std::string labels[TabCount] = {
        std::string(ic.info) + " " + i18n::tr("Information"),
        std::string(ic.output) + " " + i18n::tr("Output"),
        std::string(ic.help) + " " + i18n::tr("Help"),
        std::string(ic.snapshot) + " " + i18n::tr("Snapshots") + " (" + std::to_string(s.snapshots.size()) + ")",
    };
    Elements items;
    for (int t = 0; t < TabCount; ++t) {
        Element e = text(" " + std::to_string(t + 1) + " " + labels[t] + " ");
        e = t == s.tab ? e | inverted | bold : e | dim;
        if (boxes)
            e = e | reflect(boxes->tabs[t]);
        items.push_back(e);
        items.push_back(text(" "));
    }
    return hbox(std::move(items));
}

Element field(const std::string &label, const std::string &value)
{
    return hbox({text(label + ": ") | bold, text(value)});
}

Element infoTab(const ViewState &s)
{
    if (s.selected < 0 || s.selected >= static_cast<int>(s.rows.size()))
        return text(i18n::tr("Select a Boot Environment to see details.")) | dim;
    const bectl::BootEnvironment &be = s.rows[s.selected];
    std::string state = i18n::tr("Inactive");
    Color c = Color::Default;
    if (be.isActiveNow()) {
        state = i18n::tr("Active now");
        c = Color::Green;
    } else if (be.isTemporary()) {
        state = i18n::tr("Active on next boot only (temporary)");
        c = Color::Yellow;
    } else if (be.isActiveReboot()) {
        state = i18n::tr("Active on next boot");
        c = Color::Yellow;
    }
    return vbox({
        text(be.name) | bold,
        hbox({text(i18n::tr("State") + ": ") | bold, text(state) | color(c)}),
        field(i18n::tr("Active"), be.active),
        field(i18n::tr("Mountpoint"), be.mountpoint),
        field(i18n::tr("Space"), be.space),
        field(i18n::tr("Created"), be.created),
    });
}

Element outputTab(const ViewState &s)
{
    Elements lines;
    for (const auto &l : s.log)
        lines.push_back(text(l));
    if (!lines.empty())
        lines.back() = lines.back() | focus;
    return vbox(std::move(lines)) | yframe;
}

Element helpTab()
{
    const auto key = [](const std::string &k, const std::string &what) {
        return hbox({text(k) | bold | size(WIDTH, EQUAL, 20), text(what)});
    };
    const auto strip = [](const char *k) { return stripMnemonic(i18n::tr(k)); };
    // Duas colunas, para caber na altura da aba.
    return hbox({
        vbox({
            key("F5 / Ctrl+R", i18n::tr("Refresh")),
            key("Ctrl+N", strip("&Create…")),
            key("Enter", i18n::tr("Activate (confirmation)")),
            key("Ctrl+T", i18n::tr("Activate temporary")),
            key("F2", strip("&Rename…")),
            key("Ctrl+S", strip("Create &snapshot…")),
            key("Alt+M / Ctrl+U", i18n::tr("Mount") + " / " + i18n::tr("Unmount")),
            key("Space / Delete", i18n::tr("Mark / unmark for batch destroy") + " / " + i18n::tr("Destroy")),
        }) | flex,
        vbox({
            key("Ctrl+F  /", i18n::tr("Focus filter")),
            key("F6", i18n::tr("Sort")),
            key("1-4  Alt+1-4  F1", i18n::tr("Tabs")),
            key("Tab", i18n::tr("Switch the focused table")),
            key("Enter / Delete", i18n::tr("Clone (snapshot table)")),
            key("F9  Alt+A/C/J", i18n::tr("Open the menus")),
            key("Ctrl+Q  F10  q", i18n::tr("Quit")),
            text(i18n::tr("Click a column header to sort")) | dim,
        }) | flex,
    });
}

Element snapshotsTab(const ViewState &s)
{
    if (s.snapshots.empty())
        return text(i18n::tr("No snapshots.")) | dim;
    Elements lines;
    lines.push_back(hbox({text(i18n::tr("Snapshot")) | flex, text(i18n::tr("Space")) | align_right
                              | size(WIDTH, EQUAL, kSpaceWidth),
                          text(" "), text(i18n::tr("Created")) | size(WIDTH, EQUAL, kCreatedWidth)})
                    | bold);
    for (size_t i = 0; i < s.snapshots.size(); ++i) {
        const bectl::Snapshot &sn = s.snapshots[i];
        Element line = hbox({text(std::string(icons(s.iconMode).snapshot) + " " + sn.name) | flex,
                             text(sn.space) | align_right | size(WIDTH, EQUAL, kSpaceWidth), text(" "),
                             text(sn.created) | size(WIDTH, EQUAL, kCreatedWidth)});
        if (s.snapshotsFocused && static_cast<int>(i) == s.selectedSnapshot)
            line = line | inverted | focus;
        lines.push_back(line);
    }
    return vbox(std::move(lines)) | yframe;
}

Element statusBar(const ViewState &s)
{
    const IconSet &ic = icons(s.iconMode);
    std::string left = s.status;
    if (s.busy)
        left = std::string(ic.busy) + " " + left;
    else if (s.error)
        left = std::string(ic.error) + " " + left;
    Element l = text(left);
    if (s.error)
        l = l | color(Color::Red);
    Element marks = s.marked.empty()
                        ? text("")
                        : text(i18n::trf("%1 marked", {std::to_string(s.marked.size())}) + "  ") | color(Color::Cyan);
    return hbox({l | flex, marks, text(i18n::trf("Theme: %1", {s.themeLabel})) | dim, text("  "),
                 text(i18n::trf("%1 BE(s)", {std::to_string(s.totalEnvironments)}))});
}

Element keyBar()
{
    const auto key = [](const char *k, const std::string &label) {
        return hbox({text(k) | bold, text(label + " ") | inverted});
    };
    const auto strip = [](const char *k) { return stripMnemonic(i18n::tr(k)); };
    // Como no Midnight Commander: teclas de função numeradas.
    return hbox({key("F1", i18n::tr("Help")), text(" "), key("F2", strip("&Rename…")), text(" "),
                 key("F5", i18n::tr("Refresh")), text(" "), key("F6", i18n::tr("Sort")), text(" "),
                 key("F9", i18n::tr("Menu")), text(" "), key("F10", i18n::tr("Quit")), filler()});
}

Element button(const std::string &label, bool focused, bool danger)
{
    Element b = text("[ " + label + " ]");
    if (focused)
        b = b | inverted | bold;
    if (danger && focused)
        b = b | color(Color::Red);
    return b;
}

Element formDialog(const Dialog &d)
{
    Elements rows;
    for (size_t i = 0; i < d.fields.size(); ++i) {
        const DialogField &f = d.fields[i];
        const bool focused = static_cast<int>(i) == d.focus;
        Element value;
        switch (f.kind) {
        case DialogField::Text:
            value = hbox({text(f.text), focused ? text("█") | blink : text("")}) | size(WIDTH, GREATER_THAN, 30)
                    | borderLight;
            break;
        case DialogField::Check:
            value = text(std::string(f.checked ? "[x] " : "[ ] ") + f.label);
            break;
        case DialogField::Choice:
            value = text("< " + (f.choices.empty() ? std::string() : f.choices[f.choice]) + " >")
                    | size(WIDTH, GREATER_THAN, 30);
            break;
        }
        if (focused && f.kind != DialogField::Text)
            value = value | inverted;
        Element label = text(f.kind == DialogField::Check ? std::string() : f.label) | size(WIDTH, EQUAL, 28);
        rows.push_back(hbox({label, value}));
        if (!f.hint.empty())
            rows.push_back(hbox({text("") | size(WIDTH, EQUAL, 28), text(f.hint) | dim}));
    }
    const int ok = static_cast<int>(d.fields.size());
    rows.push_back(text(""));
    rows.push_back(hbox({filler(), button(i18n::tr("OK"), d.focus == ok, false), text("  "),
                         button(i18n::tr("Cancel"), d.focus == ok + 1, false)}));
    rows.push_back(text(i18n::tr("Tab / ↑ ↓: field   Space: check   ← →: choose   Enter: confirm   Esc: cancel"))
                   | dim);
    return window(text(" " + d.title + " ") | bold, vbox(std::move(rows)));
}

Elements paragraph(const std::string &message)
{
    Elements lines;
    size_t start = 0;
    for (;;) {
        const size_t nl = message.find('\n', start);
        lines.push_back(text(message.substr(start, nl == std::string::npos ? std::string::npos : nl - start)));
        if (nl == std::string::npos)
            break;
        start = nl + 1;
    }
    return lines;
}

Element confirmDialog(const Dialog &d)
{
    Elements rows = paragraph(d.message);
    rows.push_back(text(""));
    rows.push_back(hbox({filler(), button(i18n::tr("Yes"), d.focus == 0, d.danger), text("  "),
                         button(i18n::tr("No"), d.focus == 1, false)}));
    Element w = window(text(" " + d.title + " ") | bold, vbox(std::move(rows)));
    return d.danger ? w | color(Color::Red) : w;
}

Element messageDialog(const Dialog &d)
{
    Elements rows = paragraph(d.message);
    rows.push_back(text(""));
    rows.push_back(hbox({filler(), button(i18n::tr("OK"), true, false)}));
    return window(text(" " + d.title + " ") | bold, vbox(std::move(rows)));
}

Element menuDialog(const Dialog &d)
{
    Elements titles;
    for (size_t m = 0; m < d.menus.size(); ++m) {
        Element t = text(" " + d.menus[m] + " ");
        titles.push_back(static_cast<int>(m) == d.menu ? t | inverted | bold : t);
    }
    Elements items;
    if (d.menu >= 0 && d.menu < static_cast<int>(d.items.size())) {
        const auto &list = d.items[d.menu];
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].first.empty()) {
                items.push_back(separatorLight());
                continue;
            }
            Element e = hbox({text(" " + list[i].first) | flex, text(list[i].second + " ") | dim});
            items.push_back(static_cast<int>(i) == d.item ? e | inverted : e);
        }
    }
    return vbox({hbox(std::move(titles)), window(text(""), vbox(std::move(items)) | size(WIDTH, GREATER_THAN, 36))});
}

Element sortChooser(const ViewState &s)
{
    Elements items;
    for (int i = 0; i <= model::ColumnCount; ++i) {
        const std::string label = i == 0 ? i18n::tr("(bectl order)") : columnTitle(i - 1) + sortMark(s, i - 1);
        Element e = text(" " + label + " ");
        if (i == s.sortChooserIndex)
            e = e | inverted;
        items.push_back(e);
    }
    return window(text(" " + i18n::tr("Sort by") + " "), vbox(std::move(items))) | clear_under | center;
}

} // namespace

std::string stripMnemonic(const std::string &label)
{
    std::string out;
    for (char c : label)
        if (c != '&')
            out += c;
    return out;
}

std::string columnTitle(int column)
{
    switch (column) {
    case model::StateIcon:  return i18n::tr("State icon");
    case model::Name:       return i18n::tr("Boot Environment");
    case model::Active:     return i18n::tr("Active");
    case model::Mountpoint: return i18n::tr("Mountpoint");
    case model::Space:      return i18n::tr("Space");
    case model::Created:    return i18n::tr("Created");
    default:                return std::string();
    }
}

Element render(const ViewState &s, Element filter, HitBoxes *boxes)
{
    const IconSet &ic = icons(s.iconMode);
    Element tabContent;
    switch (s.tab) {
    case OutputTab:    tabContent = outputTab(s); break;
    case HelpTab:      tabContent = helpTab(); break;
    case SnapshotsTab: tabContent = snapshotsTab(s); break;
    default:           tabContent = infoTab(s); break;
    }
    Element main = vbox({
        hbox({text(" bectl — Boot Environment Manager ") | bold, filler(),
              text(std::string(ic.filter) + " "), filter | size(WIDTH, EQUAL, 30)}),
        separator(),
        table(s, boxes) | flex,
        separator(),
        tabs(s, boxes),
        tabContent | size(HEIGHT, EQUAL, 9),
        separator(),
        statusBar(s),
        keyBar(),
    });
    switch (s.dialog.kind) {
    case Dialog::Form:    return dbox({main, formDialog(s.dialog) | clear_under | center});
    case Dialog::Confirm: return dbox({main, confirmDialog(s.dialog) | clear_under | center});
    case Dialog::Message: return dbox({main, messageDialog(s.dialog) | clear_under | center});
    case Dialog::Menu:    return dbox({main, menuDialog(s.dialog) | clear_under});
    case Dialog::None:    break;
    }
    if (s.sortChooserOpen)
        return dbox({main, sortChooser(s)});
    return main;
}

} // namespace tui
