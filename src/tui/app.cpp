#include "app.h"
#include "i18n.h"
#include "process.h"

#include <cstdio>
#include <ctime>
#include <ftxui/component/component.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <memory>
#include <unistd.h>

using namespace ftxui;

namespace tui {

namespace {

// Teclas que o FTXUI entrega como sequência crua.
const std::string kCtrlF = "\x06";
const std::string kCtrlN = "\x0e";
const std::string kCtrlQ = "\x11";
const std::string kCtrlR = "\x12";
const std::string kCtrlS = "\x13";
const std::string kCtrlT = "\x14";
const std::string kCtrlU = "\x15";
const std::string kAltA = "\x1b" "a";
const std::string kAltC = "\x1b" "c";
const std::string kAltJ = "\x1b" "j";
const std::string kAltM = "\x1b" "m";

enum Menus { FileMenu = 0, ActionsMenu, HelpMenu };

std::string defaultElevator()
{
    return ::geteuid() == 0 ? std::string() : std::string("doas");
}

std::string trim(const std::string &s)
{
    const auto b = s.find_first_not_of(" \t");
    if (b == std::string::npos)
        return std::string();
    return s.substr(b, s.find_last_not_of(" \t") - b + 1);
}

std::string bullets(const std::vector<std::string> &items)
{
    std::string out;
    for (size_t i = 0; i < items.size(); ++i)
        out += (i ? "\n• " : "") + items[i];
    return out;
}

std::string label(const char *key)
{
    return stripMnemonic(i18n::tr(key));
}

DialogField makeField(DialogField::Kind kind, const std::string &label)
{
    DialogField f;
    f.kind = kind;
    f.label = label;
    return f;
}

// Remove o último caractere UTF-8 (Backspace no campo de texto).
void popUtf8(std::string &s)
{
    while (!s.empty()) {
        const unsigned char c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0) != 0x80)
            break;
    }
}

} // namespace

App::App(const Options &options)
    : m_settings(options.settingsPath)
    , m_mgr(options.elevator == "-" ? m_settings.value("elevator", defaultElevator()) : options.elevator,
            options.bectlPath)
    , m_iconMode(iconModeFromString(options.iconMode.empty() ? m_settings.value("icons", "nerd")
                                                             : options.iconMode))
{
    const std::string lang = options.language.empty() ? m_settings.value("language", "system")
                                                      : options.language;
    if (lang == "system")
        i18n::initFromEnvironment();
    else
        i18n::setLocale(lang);
    m_sort.column = m_settings.intValue("sort.column", -1);
    if (m_sort.column < -1 || m_sort.column >= model::ColumnCount)
        m_sort.column = -1;
    m_sort.ascending = m_settings.value("sort.order", "asc") != "desc";
    m_status = i18n::tr("Ready.");
}

App::~App()
{
    if (m_worker.joinable())
        m_worker.join();
}

void App::log(const std::string &message)
{
    char ts[16];
    const std::time_t now = std::time(nullptr);
    std::strftime(ts, sizeof ts, "%H:%M:%S", std::localtime(&now));
    m_log.push_back(std::string("[") + ts + "] " + message);
    if (m_log.size() > 500)
        m_log.erase(m_log.begin());
}

// ---------------------------------------------------------------------------
// Listagem
// ---------------------------------------------------------------------------

// Lista em segundo plano: a tela continua respondendo enquanto o bectl roda.
void App::refresh()
{
    if (m_busy)
        return;
    m_busy = true;
    m_error = false;
    m_status = i18n::tr("Listing Boot Environments…");
    const auto list = [this](std::vector<bectl::BootEnvironment> &l, std::vector<bectl::Snapshot> &s) {
        try {
            l = m_mgr.listEnvironments();
            s = m_mgr.listSnapshots();
        } catch (const std::exception &e) {
            return std::string(e.what());
        }
        return std::string();
    };
    if (!m_screen) {
        std::vector<bectl::BootEnvironment> l;
        std::vector<bectl::Snapshot> s;
        const std::string error = list(l, s);
        applyListing(l, s, error);
        return;
    }
    if (m_worker.joinable())
        m_worker.join();
    m_worker = std::thread([this, list] {
        std::vector<bectl::BootEnvironment> l;
        std::vector<bectl::Snapshot> s;
        const std::string error = list(l, s);
        m_screen->Post([this, l, s, error] { applyListing(l, s, error); });
        m_screen->PostEvent(Event::Custom);
    });
}

void App::applyListing(const std::vector<bectl::BootEnvironment> &list,
                       const std::vector<bectl::Snapshot> &snapshots, const std::string &error)
{
    m_busy = false;
    if (!error.empty()) {
        m_error = true;
        m_status = i18n::trf("ERROR: %1", {error});
        log(m_status);
        return;
    }
    m_all = list;
    m_snapshots = snapshots;
    // Marcas de BEs que deixaram de existir saem.
    for (auto it = m_marked.begin(); it != m_marked.end();) {
        const bool exists = std::any_of(m_all.begin(), m_all.end(),
                                        [&](const bectl::BootEnvironment &be) { return be.name == *it; });
        it = exists ? std::next(it) : m_marked.erase(it);
    }
    m_status = i18n::tr("Ready.");
    log(i18n::trf("List updated (%1 BE(s)).", {std::to_string(list.size())}));
}

// ---------------------------------------------------------------------------
// Operações
// ---------------------------------------------------------------------------

bool App::refuseWhileBusy()
{
    if (!m_busy)
        return false;
    m_status = i18n::tr("Wait for the current operation to finish.");
    return true;
}

// O doas/sudo pede senha? Testa com -n (não interativo) e uma leitura
// inofensiva do próprio bectl, que é o que a regra do doas.conf costuma liberar.
bool App::passwordNeeded()
{
    if (m_mgr.elevator().empty())
        return false;
    const std::string tool = bectl::findExecutable(m_mgr.elevator());
    if (tool.empty())
        return false;   // a operação informa que o elevador não existe
    const auto r = bectl::runProcess({tool, "-n", m_mgr.bectlPath(), "list", "-H"}, 10000);
    return !r.started || r.status != 0;
}

void App::runOperation(const std::string &status, const std::string &command, const Op &op,
                       const Op &onSuccess)
{
    if (refuseWhileBusy())
        return;
    log(command);
    const auto attempt = [op] {
        try {
            op();
        } catch (const std::exception &e) {
            return std::string(e.what());
        }
        return std::string();
    };

    if (!m_screen) {   // testes
        finishOperation(attempt(), onSuccess);
        return;
    }

    if (passwordNeeded()) {
        // O doas/sudo pede a senha no terminal: a tela é suspensa enquanto isso.
        std::string error;
        m_screen->WithRestoredIO([&] {
            std::printf("\nbectl-tui — %s\n%s\n\n", i18n::trf("Running: %1", {command}).c_str(),
                        i18n::trf("%1 may ask for your password.", {m_mgr.elevator()}).c_str());
            std::fflush(stdout);
            error = attempt();
        })();
        finishOperation(error, onSuccess);
        return;
    }

    m_busy = true;
    m_error = false;
    m_status = status;
    if (m_worker.joinable())
        m_worker.join();
    m_worker = std::thread([this, attempt, onSuccess] {
        const std::string error = attempt();
        m_screen->Post([this, error, onSuccess] { finishOperation(error, onSuccess); });
        m_screen->PostEvent(Event::Custom);
    });
}

void App::finishOperation(const std::string &error, const Op &onSuccess)
{
    m_busy = false;
    if (!error.empty()) {
        m_error = true;
        m_status = i18n::trf("ERROR: %1", {error});
        log(m_status);
        showMessage(i18n::tr("Error"), error);
        return;
    }
    m_error = false;
    m_status = i18n::tr("Ready.");
    if (onSuccess)
        onSuccess();
    refresh();
}

std::string App::currentName()
{
    const auto rows = model::visibleRows(m_all, m_filter, m_sort);
    const int i = model::rowToSelect(rows, m_selectedName);
    return i >= 0 ? rows[i].name : std::string();
}

const bectl::BootEnvironment *App::current()
{
    const std::string name = currentName();
    for (const auto &be : m_all)
        if (be.name == name)
            return &be;
    return nullptr;
}

std::string App::currentSnapshot()
{
    if (!m_snapshotsFocused)
        return std::string();
    const auto snaps = model::snapshotsOf(m_snapshots, currentName());
    if (snaps.empty())
        return std::string();
    const int i = std::max(0, std::min(m_selectedSnapshot, static_cast<int>(snaps.size()) - 1));
    return snaps[i].name;
}

void App::actCreate()
{
    if (refuseWhileBusy())
        return;
    // Origem: o BE atual, outro BE ou um snapshot (no bectl-gui, digitado).
    std::vector<std::string> sources{i18n::tr("(Current Boot Environment)")};
    for (const auto &be : m_all)
        sources.push_back(be.name);
    for (const auto &s : m_snapshots)
        sources.push_back(s.name);
    DialogField name = makeField(DialogField::Text, i18n::tr("Name:"));
    DialogField from = makeField(DialogField::Choice, i18n::tr("Clone from:"));
    from.choices = sources;
    DialogField recursive = makeField(DialogField::Check, i18n::tr("Create recursively (-r)"));
    showForm(i18n::tr("Create Boot Environment"), {name, from, recursive},
             [this](const std::vector<DialogField> &f) {
                 const std::string n = trim(f[0].text);
                 if (n.empty()) {
                     showMessage(i18n::tr("Name"), i18n::tr("Please enter a name."));
                     return;
                 }
                 const std::string source = f[1].choice == 0 ? std::string() : f[1].choices[f[1].choice];
                 const bool rec = f[2].checked;
                 runOperation(i18n::tr("Creating…"), "bectl create … " + n,
                              [this, n, source, rec] { m_mgr.create(n, source, rec); },
                              [this, n] {
                                  log(i18n::trf("OK: '%1' created.", {n}));
                                  showMessage(i18n::tr("Success"),
                                              i18n::trf("Boot Environment '%1' created.", {n}));
                              });
             });
}

void App::actActivate(bool temporary)
{
    if (refuseWhileBusy())
        return;
    const std::string name = currentName();
    if (name.empty()) {
        showMessage(i18n::tr("Selection"), i18n::tr("Select exactly one Boot Environment."));
        return;
    }
    const std::string question = temporary ? i18n::trf("Activate '%1' temporarily?", {name})
                                           : i18n::trf("Activate '%1' permanently?", {name});
    showConfirm(temporary ? i18n::tr("Activate (temporary)") : i18n::tr("Activate"), question, false,
                [this, name, temporary] {
                    runOperation(i18n::tr("Activating…"),
                                 std::string("bectl activate ") + (temporary ? "-t " : "") + name,
                                 [this, name, temporary] { m_mgr.activate(name, temporary); },
                                 [this, temporary] {
                                     log(temporary ? i18n::tr("OK: activated (temporary).")
                                                   : i18n::tr("OK: activated."));
                                 });
                });
}

void App::actRename()
{
    if (refuseWhileBusy())
        return;
    const std::string name = currentName();
    if (name.empty()) {
        showMessage(i18n::tr("Selection"), i18n::tr("Select exactly one Boot Environment."));
        return;
    }
    DialogField field = makeField(DialogField::Text, i18n::tr("New name:"));
    field.text = name;
    showForm(i18n::tr("Rename"), {field}, [this, name](const std::vector<DialogField> &f) {
        const std::string neu = trim(f[0].text);
        if (neu.empty() || neu == name)
            return;
        runOperation(i18n::tr("Renaming…"), "bectl rename " + name + " " + neu,
                     [this, name, neu] { m_mgr.rename(name, neu); },
                     [this, neu] {
                         log(i18n::tr("OK: renamed."));
                         m_selectedName = neu;   // a seleção acompanha o BE renomeado
                     });
    });
}

void App::actDestroy()
{
    if (refuseWhileBusy())
        return;
    std::vector<std::string> names(m_marked.begin(), m_marked.end());
    if (names.empty() && !currentName().empty())
        names.push_back(currentName());
    if (names.empty()) {
        showMessage(i18n::tr("Selection"), i18n::tr("Select one or more Boot Environments."));
        return;
    }
    // O BE em uso fica protegido, como no bectl-gui.
    std::vector<std::string> toDestroy, blocked;
    for (const auto &n : names) {
        const auto it = std::find_if(m_all.begin(), m_all.end(),
                                     [&](const bectl::BootEnvironment &be) { return be.name == n; });
        (it != m_all.end() && it->isActiveNow() ? blocked : toDestroy).push_back(n);
    }
    const std::string skipped = blocked.empty() ? std::string()
                                                : i18n::trf("Skipped (active):\n• %1", {bullets(blocked)});
    if (toDestroy.empty()) {
        showMessage(i18n::tr("Active BE"), skipped);
        return;
    }
    const std::string warning =
        toDestroy.size() == 1
            ? i18n::trf("WARNING: destroying '%1' is irreversible.\nContinue?", {toDestroy.front()})
            : i18n::trf("WARNING: destroying %1 Boot Environments is irreversible:\n• %2\nContinue?",
                        {std::to_string(toDestroy.size()), bullets(toDestroy)});
    showConfirm(i18n::tr("Confirm destroy"), skipped.empty() ? warning : skipped + "\n\n" + warning, true,
                [this, toDestroy] {
                    std::string list;
                    for (const auto &n : toDestroy)
                        list += " " + n;
                    auto result = std::make_shared<bectl::DestroyManyResult>();
                    runOperation(i18n::tr("Destroying…"), "bectl destroy -F" + list,
                                 [this, toDestroy, result] { *result = m_mgr.destroyMany(toDestroy, true, false, true); },
                                 [this, result] {
                                     std::string report;
                                     if (!result->ok.empty())
                                         report += i18n::trf("Destroyed (%1):\n• %2\n",
                                                             {std::to_string(result->ok.size()), bullets(result->ok)});
                                     if (!result->errors.empty())
                                         report += i18n::trf("Failures (%1):\n• %2",
                                                             {std::to_string(result->errors.size()),
                                                              bullets(result->errors)});
                                     while (!report.empty() && report.back() == '\n')
                                         report.pop_back();
                                     log(report);
                                     for (const auto &n : result->ok)
                                         m_marked.erase(n);
                                     showMessage(i18n::tr("Result"), report.empty() ? i18n::tr("Done.") : report);
                                 });
                });
}

void App::actMount()
{
    if (refuseWhileBusy())
        return;
    const bectl::BootEnvironment *be = current();
    if (!be) {
        showMessage(i18n::tr("Selection"), i18n::tr("Select exactly one Boot Environment."));
        return;
    }
    if (be->isMounted())
        return;   // como no bectl-gui: Montar só vale para BE não montado
    const std::string name = be->name;
    DialogField dir = makeField(DialogField::Text, i18n::tr("Mount point:"));
    dir.hint = i18n::tr("Leave empty to let bectl create a temporary directory.");
    showForm(i18n::tr("Mount Boot Environment"), {dir}, [this, name](const std::vector<DialogField> &f) {
        const std::string d = trim(f[0].text);
        auto out = std::make_shared<std::string>();
        runOperation(i18n::tr("Mounting…"), "bectl mount " + name + (d.empty() ? "" : " " + d),
                     [this, name, d, out] { *out = m_mgr.mount(name, d); },
                     [this, name, out] {
                         if (!out->empty())
                             log(*out);
                         log(i18n::trf("OK: '%1' mounted.", {name}));
                     });
    });
}

void App::actUnmount()
{
    if (refuseWhileBusy())
        return;
    const bectl::BootEnvironment *be = current();
    // O BE em uso está montado em "/" e não pode ser desmontado.
    if (!be || !be->isMounted() || be->isActiveNow())
        return;
    const std::string name = be->name;
    runOperation(i18n::tr("Unmounting…"), "bectl umount " + name, [this, name] { m_mgr.unmount(name); },
                 [this, name] { log(i18n::trf("OK: '%1' unmounted.", {name})); });
}

void App::actSnapshot()
{
    if (refuseWhileBusy())
        return;
    DialogField be = makeField(DialogField::Choice, i18n::tr("Boot Environment:"));
    for (const auto &e : m_all)
        be.choices.push_back(e.name);
    if (be.choices.empty())
        return;
    const auto it = std::find(be.choices.begin(), be.choices.end(), currentName());
    be.choice = it == be.choices.end() ? 0 : static_cast<int>(it - be.choices.begin());
    DialogField name = makeField(DialogField::Text, i18n::tr("Snapshot name:"));
    DialogField recursive = makeField(DialogField::Check, i18n::tr("Create recursively (-r)"));
    showForm(i18n::tr("Create snapshot"), {be, name, recursive}, [this](const std::vector<DialogField> &f) {
        const std::string snap = trim(f[1].text);
        if (snap.empty()) {
            showMessage(i18n::tr("Name"), i18n::tr("Please enter a name."));
            return;
        }
        const std::string full = f[0].choices[f[0].choice] + "@" + snap;
        const bool rec = f[2].checked;
        runOperation(i18n::tr("Creating snapshot…"), std::string("bectl create ") + (rec ? "-r " : "") + full,
                     [this, full, rec] { m_mgr.createSnapshot(full, rec); },
                     [this, full] { log(i18n::trf("OK: snapshot '%1' created.", {full})); });
    });
}

void App::actCloneSnapshot()
{
    if (refuseWhileBusy())
        return;
    const std::string snap = currentSnapshot();
    if (snap.empty()) {
        m_tab = SnapshotsTab;
        showMessage(i18n::tr("Selection"), i18n::tr("Select a snapshot."));
        return;
    }
    DialogField name = makeField(DialogField::Text, i18n::tr("New Boot Environment name:"));
    showForm(i18n::tr("Clone snapshot"), {name}, [this, snap](const std::vector<DialogField> &f) {
        const std::string n = trim(f[0].text);
        if (n.empty())
            return;
        runOperation(i18n::tr("Cloning…"), "bectl create -e " + snap + " " + n,
                     [this, n, snap] { m_mgr.create(n, snap); },
                     [this, n, snap] { log(i18n::trf("OK: '%1' created from '%2'.", {n, snap})); });
    });
}

void App::actDestroySnapshot()
{
    if (refuseWhileBusy())
        return;
    const std::string snap = currentSnapshot();
    if (snap.empty()) {
        m_tab = SnapshotsTab;
        showMessage(i18n::tr("Selection"), i18n::tr("Select a snapshot."));
        return;
    }
    showConfirm(i18n::tr("Confirm destroy"),
                i18n::trf("WARNING: destroying '%1' is irreversible.\nContinue?", {snap}), true, [this, snap] {
                    runOperation(i18n::tr("Destroying…"), "bectl destroy " + snap,
                                 [this, snap] { m_mgr.destroy(snap); },
                                 [this, snap] { log(i18n::trf("OK: snapshot '%1' destroyed.", {snap})); });
                });
}

void App::actAbout()
{
    showMessage(label("About bectl…"), std::string("bectl-tui ") + BECTL_TUI_VERSION
                                           + "\nbectl(8) · FTXUI\nhttps://github.com/cleberalves40/bectl-tui");
}

// ---------------------------------------------------------------------------
// Diálogos e menus
// ---------------------------------------------------------------------------

void App::showMessage(const std::string &title, const std::string &message)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Message;
    m_dialog.title = title;
    m_dialog.message = message;
}

void App::showConfirm(const std::string &title, const std::string &message, bool danger, const Op &onYes)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Confirm;
    m_dialog.title = title;
    m_dialog.message = message;
    m_dialog.danger = danger;
    m_dialog.focus = danger ? 1 : 0;   // destruir: "Não" por padrão, como no bectl-gui
    m_onAccept = onYes;
}

void App::showForm(const std::string &title, std::vector<DialogField> fields, FormOp onAccept)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Form;
    m_dialog.title = title;
    m_dialog.fields = std::move(fields);
    m_onForm = std::move(onAccept);
}

void App::openMenu(int menu)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Menu;
    m_dialog.menus = {label("&File"), label("&Actions"), label("&Help")};
    const auto sep = std::make_pair(std::string(), std::string());
    m_dialog.items = {
        {{label("&Refresh"), "F5"}, sep, {label("&Quit"), "Ctrl+Q"}},
        {{label("&Create…"), "Ctrl+N"},
         {label("A&ctivate"), "Enter"},
         {label("Activate (&temporary)"), "Ctrl+T"},
         {label("&Rename…"), "F2"},
         sep,
         {label("Create &snapshot…"), "Ctrl+S"},
         {label("C&lone snapshot…"), "Enter"},
         {label("Destroy s&napshot"), "Del"},
         sep,
         {label("&Mount…"), "Alt+M"},
         {label("&Unmount"), "Ctrl+U"},
         sep,
         {label("&Destroy selected"), "Del"}},
        {{i18n::tr("Keyboard shortcuts"), "F1"}, {label("About bectl…"), ""}},
    };
    m_menuActions = {
        {[this] { refresh(); }, nullptr, [this] { quit(); }},
        {[this] { actCreate(); }, [this] { actActivate(false); }, [this] { actActivate(true); },
         [this] { actRename(); }, nullptr, [this] { actSnapshot(); }, [this] { actCloneSnapshot(); },
         [this] { actDestroySnapshot(); }, nullptr, [this] { actMount(); }, [this] { actUnmount(); }, nullptr,
         [this] { actDestroy(); }},
        {[this] { m_tab = HelpTab; }, [this] { actAbout(); }},
    };
    m_dialog.menu = std::max(0, std::min(menu, static_cast<int>(m_dialog.menus.size()) - 1));
}

bool App::handleDialogEvent(const Event &event)
{
    Dialog &d = m_dialog;
    switch (d.kind) {
    case Dialog::Message:
        if (event == Event::Return || event == Event::Escape || event == Event::Character(' '))
            d = Dialog{};
        return true;

    case Dialog::Confirm:
        if (event == Event::ArrowLeft || event == Event::ArrowRight || event == Event::Tab
            || event == Event::TabReverse)
            d.focus = 1 - d.focus;
        else if (event == Event::Escape || event == Event::Character('n') || event == Event::Character('N'))
            d = Dialog{};
        else if (event == Event::Return || event == Event::Character('y') || event == Event::Character('s')
                 || event == Event::Character('Y') || event == Event::Character('S')) {
            const bool yes = event != Event::Return || d.focus == 0;
            const Op cb = m_onAccept;
            d = Dialog{};
            if (yes && cb)
                cb();
        }
        return true;

    case Dialog::Form: {
        const int total = static_cast<int>(d.fields.size()) + 2;   // campos + OK + Cancelar
        if (event == Event::Escape) {
            d = Dialog{};
            return true;
        }
        if (event == Event::Tab || event == Event::ArrowDown) {
            d.focus = (d.focus + 1) % total;
            return true;
        }
        if (event == Event::TabReverse || event == Event::ArrowUp) {
            d.focus = (d.focus + total - 1) % total;
            return true;
        }
        if (event == Event::Return) {
            if (d.focus == total - 1) {   // Cancelar
                d = Dialog{};
                return true;
            }
            const std::vector<DialogField> fields = d.fields;
            const FormOp cb = m_onForm;
            d = Dialog{};
            if (cb)
                cb(fields);
            return true;
        }
        if (d.focus >= static_cast<int>(d.fields.size()))
            return true;
        DialogField &f = d.fields[d.focus];
        switch (f.kind) {
        case DialogField::Text:
            if (event == Event::Backspace)
                popUtf8(f.text);
            else if (event.is_character())
                f.text += event.character();
            break;
        case DialogField::Check:
            if (event == Event::Character(' '))
                f.checked = !f.checked;
            break;
        case DialogField::Choice:
            if (!f.choices.empty() && (event == Event::ArrowRight || event == Event::Character(' ')))
                f.choice = (f.choice + 1) % static_cast<int>(f.choices.size());
            else if (!f.choices.empty() && event == Event::ArrowLeft)
                f.choice = (f.choice + static_cast<int>(f.choices.size()) - 1) % static_cast<int>(f.choices.size());
            break;
        }
        return true;
    }

    case Dialog::Menu: {
        const int menus = static_cast<int>(d.menus.size());
        const auto &items = d.items[d.menu];
        const auto step = [&](int delta) {
            const int n = static_cast<int>(items.size());
            for (int i = 0; i < n; ++i) {
                d.item = (d.item + delta + n) % n;
                if (!items[d.item].first.empty())
                    break;   // separadores não são selecionáveis
            }
        };
        if (event == Event::Escape || event == Event::F9 || event == Event::F10)
            d = Dialog{};
        else if (event == Event::ArrowLeft) {
            d.menu = (d.menu + menus - 1) % menus;
            d.item = 0;
        } else if (event == Event::ArrowRight) {
            d.menu = (d.menu + 1) % menus;
            d.item = 0;
        } else if (event == Event::ArrowUp)
            step(-1);
        else if (event == Event::ArrowDown)
            step(1);
        else if (event == Event::Return) {
            const Op action = m_menuActions[d.menu][d.item];
            d = Dialog{};
            if (action)
                action();
        }
        return true;
    }

    case Dialog::None:
        break;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Estado, teclado e mouse
// ---------------------------------------------------------------------------

ViewState App::viewState()
{
    ViewState s;
    s.rows = model::visibleRows(m_all, m_filter, m_sort);
    // A seleção acompanha o BE, não a linha: sobrevive a filtro, ordem e atualização.
    s.selected = model::rowToSelect(s.rows, m_selectedName);
    m_selectedName = s.selected >= 0 ? s.rows[s.selected].name : std::string();
    s.sort = m_sort;
    s.snapshots = model::snapshotsOf(m_snapshots, m_selectedName);
    if (m_selectedSnapshot >= static_cast<int>(s.snapshots.size()))
        m_selectedSnapshot = static_cast<int>(s.snapshots.size()) - 1;
    if (m_selectedSnapshot < 0)
        m_selectedSnapshot = 0;
    if (s.snapshots.empty())
        m_snapshotsFocused = false;
    s.selectedSnapshot = m_selectedSnapshot;
    s.snapshotsFocused = m_snapshotsFocused;
    s.tab = m_tab;
    s.log = m_log;
    s.status = m_status;
    s.busy = m_busy;
    s.error = m_error;
    s.totalEnvironments = static_cast<int>(m_all.size());
    s.themeLabel = i18n::tr("System");
    s.iconMode = m_iconMode;
    s.sortChooserOpen = m_sortChooser;
    s.sortChooserIndex = m_sortChooserIndex;
    s.marked = m_marked;
    s.dialog = m_dialog;
    return s;
}

void App::setSort(model::SortSpec sort)
{
    m_sort = sort;
    m_settings.setValue("sort.column", m_sort.column);
    m_settings.setValue("sort.order", m_sort.ascending ? "asc" : "desc");
    m_settings.save();
}

void App::move(int delta)
{
    if (m_snapshotsFocused) {
        m_selectedSnapshot += delta;   // limitado em viewState()
        return;
    }
    const auto rows = model::visibleRows(m_all, m_filter, m_sort);
    if (rows.empty())
        return;
    int i = model::rowToSelect(rows, m_selectedName) + delta;
    i = std::max(0, std::min(i, static_cast<int>(rows.size()) - 1));
    m_selectedName = rows[i].name;
    m_selectedSnapshot = 0;
}

void App::quit()
{
    if (m_busy) {
        m_status = i18n::tr("Wait for the current operation to finish.");
        return;
    }
    if (m_screen)
        m_screen->Exit();
}

bool App::handleMouse(const Event &event)
{
    Event copy = event;   // mouse() não é const em todas as versões do FTXUI
    Mouse mouse = copy.mouse();
    if (mouse.button == Mouse::WheelUp) {
        move(-1);
        return true;
    }
    if (mouse.button == Mouse::WheelDown) {
        move(1);
        return true;
    }
    if (mouse.button != Mouse::Left || mouse.motion != Mouse::Pressed)
        return true;
    for (int c = 0; c < model::ColumnCount; ++c)
        if (m_boxes.header[c].Contain(mouse.x, mouse.y)) {
            setSort(model::nextOnHeaderClick(m_sort, c));
            return true;
        }
    for (int t = 0; t < TabCount; ++t)
        if (m_boxes.tabs[t].Contain(mouse.x, mouse.y)) {
            m_tab = t;
            return true;
        }
    const auto rows = model::visibleRows(m_all, m_filter, m_sort);
    for (size_t i = 0; i < m_boxes.rows.size() && i < rows.size(); ++i)
        if (m_boxes.rows[i].Contain(mouse.x, mouse.y)) {
            m_selectedName = rows[i].name;
            m_snapshotsFocused = false;
            return true;
        }
    return true;
}

bool App::handleEvent(const Event &event)
{
    if (m_dialog.kind != Dialog::None)
        return handleDialogEvent(event);

    // Escolha da coluna de ordenação (F6).
    if (m_sortChooser) {
        if (event == Event::ArrowUp || event == Event::Character('k'))
            m_sortChooserIndex = std::max(0, m_sortChooserIndex - 1);
        else if (event == Event::ArrowDown || event == Event::Character('j'))
            m_sortChooserIndex = std::min(static_cast<int>(model::ColumnCount), m_sortChooserIndex + 1);
        else if (event == Event::Return) {
            const int column = m_sortChooserIndex - 1;
            if (column < 0)
                setSort({-1, true});
            else if (column == m_sort.column)
                setSort({column, !m_sort.ascending});
            else
                setSort({column, true});
            m_sortChooser = false;
        } else if (event == Event::Escape || event == Event::F6)
            m_sortChooser = false;
        return true;
    }

    // Campo de filtro: Esc ou Enter devolvem o foco à tabela; o resto vai ao Input.
    if (m_filterMode) {
        if (event == Event::Escape || event == Event::Return) {
            m_filterMode = false;
            return true;
        }
        if (event.is_mouse())
            return handleMouse(event);
        return false;
    }

    if (event.is_mouse())
        return handleMouse(event);

    const std::string &raw = event.input();

    // Operações (veja docs/KEYMAP.md).
    if (raw == kCtrlN) {
        actCreate();
        return true;
    }
    if (event == Event::Return) {
        m_snapshotsFocused ? actCloneSnapshot() : actActivate(false);
        return true;
    }
    if (raw == kCtrlT) {
        actActivate(true);
        return true;
    }
    if (event == Event::F2) {
        actRename();
        return true;
    }
    if (raw == kCtrlS) {
        actSnapshot();
        return true;
    }
    if (raw == kAltM) {
        actMount();
        return true;
    }
    if (raw == kCtrlU) {
        actUnmount();
        return true;
    }
    if (event == Event::Delete) {
        m_snapshotsFocused ? actDestroySnapshot() : actDestroy();
        return true;
    }
    if (event == Event::Character(' ') && !m_snapshotsFocused) {
        const std::string name = currentName();
        if (!name.empty() && !m_marked.erase(name))
            m_marked.insert(name);
        return true;
    }
    if (event == Event::F9 || raw == kAltC) {
        openMenu(ActionsMenu);
        return true;
    }
    if (raw == kAltA) {
        openMenu(FileMenu);
        return true;
    }
    if (raw == kAltJ) {
        openMenu(HelpMenu);
        return true;
    }

    // Navegação e listagem.
    if (event == Event::F5 || raw == kCtrlR) {
        refresh();
        return true;
    }
    if (event == Event::F6) {
        m_sortChooser = true;
        m_sortChooserIndex = m_sort.column + 1;
        return true;
    }
    if (raw == kCtrlF || event == Event::Character('/')) {
        m_filterMode = true;
        return true;
    }
    if (raw == kCtrlQ || event == Event::F10 || event == Event::Character('q')) {
        quit();
        return true;
    }
    if (event == Event::F1) {
        m_tab = HelpTab;
        return true;
    }
    for (int t = 0; t < TabCount; ++t) {
        const std::string digit(1, static_cast<char>('1' + t));
        // Ctrl+1…4 não chega pelo terminal: 1…4 e Alt+1…4 trocam de aba.
        if (raw == digit || raw == "\x1b" + digit) {
            m_tab = t;
            if (t != SnapshotsTab)
                m_snapshotsFocused = false;
            return true;
        }
    }
    if (event == Event::Tab || event == Event::TabReverse) {
        m_tab = SnapshotsTab;
        m_snapshotsFocused = !m_snapshotsFocused
                             && !model::snapshotsOf(m_snapshots, currentName()).empty();
        return true;
    }
    if (event == Event::ArrowUp || event == Event::Character('k'))
        move(-1);
    else if (event == Event::ArrowDown || event == Event::Character('j'))
        move(1);
    else if (event == Event::PageUp)
        move(-10);
    else if (event == Event::PageDown)
        move(10);
    else if (event == Event::Home)
        move(-1000000);
    else if (event == Event::End)
        move(1000000);
    return true;
}

int App::run()
{
    auto screen = ScreenInteractive::Fullscreen();
    m_screen = &screen;

    InputOption option;
    option.multiline = false;
    m_input = Input(&m_filter, i18n::tr("Filter by name…"), option);

    auto renderer = Renderer(m_input, [this] {
        // A primeira listagem parte da primeira renderização, já dentro do
        // laço: no FTXUI 6, uma tarefa enviada com Post() antes de Loop() é
        // descartada, e a lista nunca aparecia.
        if (!m_started) {
            m_started = true;
            refresh();
        }
        const ViewState state = viewState();
        Element filter = m_filterMode ? m_input->Render()
                                      : (m_filter.empty() ? text(i18n::tr("Filter by name…")) | dim
                                                          : text(m_filter));
        return render(state, filter, &m_boxes);
    });
    auto root = CatchEvent(renderer, [this](Event event) { return handleEvent(event); });

    screen.Loop(root);

    if (m_worker.joinable())
        m_worker.join();
    m_screen = nullptr;
    return 0;
}

} // namespace tui
