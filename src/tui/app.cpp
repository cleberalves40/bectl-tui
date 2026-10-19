#include "app.h"
#include "i18n.h"

#include <ctime>
#include <ftxui/component/component.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <unistd.h>

using namespace ftxui;

namespace tui {

namespace {

// Teclas que o FTXUI entrega como sequência crua.
const std::string kCtrlF = "\x06";
const std::string kCtrlQ = "\x11";
const std::string kCtrlR = "\x12";

std::string defaultElevator()
{
    return ::geteuid() == 0 ? std::string() : std::string("doas");
}

} // namespace

App::App(const Options &options)
    : m_settings(options.settingsPath)
    , m_mgr(m_settings.value("elevator", defaultElevator()), options.bectlPath)
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

// Lista em segundo plano: a tela continua respondendo enquanto o bectl roda.
void App::refresh()
{
    if (m_busy)
        return;
    m_busy = true;
    m_error = false;
    m_status = i18n::tr("Listing Boot Environments…");
    if (m_worker.joinable())
        m_worker.join();
    m_worker = std::thread([this] {
        std::vector<bectl::BootEnvironment> list;
        std::vector<bectl::Snapshot> snaps;
        std::string error;
        try {
            list = m_mgr.listEnvironments();
            snaps = m_mgr.listSnapshots();
        } catch (const std::exception &e) {
            error = e.what();
        }
        if (m_screen) {
            m_screen->Post([this, list, snaps, error] { applyListing(list, snaps, error); });
            m_screen->PostEvent(Event::Custom);
        } else {
            applyListing(list, snaps, error);
        }
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
    m_status = i18n::tr("Ready.");
    log(i18n::trf("List updated (%1 BE(s)).", {std::to_string(list.size())}));
}

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
                             && !model::snapshotsOf(m_snapshots, m_selectedName).empty();
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
        const ViewState state = viewState();
        Element filter = m_filterMode ? m_input->Render()
                                      : (m_filter.empty() ? text(i18n::tr("Filter by name…")) | dim
                                                          : text(m_filter));
        return render(state, filter, &m_boxes);
    });
    auto root = CatchEvent(renderer, [this](Event event) { return handleEvent(event); });

    screen.Post([this] { refresh(); });
    screen.Loop(root);

    if (m_worker.joinable())
        m_worker.join();
    m_screen = nullptr;
    return 0;
}

} // namespace tui
