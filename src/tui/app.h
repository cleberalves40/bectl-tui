#ifndef BECTL_TUI_APP_H
#define BECTL_TUI_APP_H

#include "bectl.h"
#include "icons.h"
#include "listmodel.h"
#include "settings.h"
#include "view.h"

#include <atomic>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
// FTXUI 7 define ScreenInteractive como apelido de ftxui::App; o cabeçalho
// real evita conflito com uma declaração antecipada.
#include <ftxui/component/screen_interactive.hpp>
#include <string>
#include <thread>
#include <vector>

namespace tui {

struct Options {
    std::string bectlPath;       // vazio: bectl(8) do PATH
    std::string language;        // vazio: preferência salva ou ambiente
    std::string iconMode;        // vazio: preferência salva ou "nerd"
    std::string settingsPath;    // vazio: ~/.config/bectl-tui/config
};

class App
{
public:
    explicit App(const Options &options);
    ~App();

    /** Laço da interface em tela cheia; devolve o código de saída. */
    int run();

    // Para os testes: o estado que a tela mostraria e o tratamento de eventos.
    ViewState viewState();
    bool handleEvent(const ftxui::Event &event);
    void applyListing(const std::vector<bectl::BootEnvironment> &list,
                      const std::vector<bectl::Snapshot> &snapshots, const std::string &error);
    bool filterMode() const { return m_filterMode; }
    std::string &filterText() { return m_filter; }
    const bectl::Settings &settings() const { return m_settings; }

private:
    void refresh();
    void log(const std::string &message);
    void setSort(model::SortSpec sort);
    void move(int delta);
    bool handleMouse(const ftxui::Event &event);
    void quit();

    bectl::Settings m_settings;
    bectl::Manager m_mgr;
    IconMode m_iconMode;

    std::vector<bectl::BootEnvironment> m_all;   // última listagem
    std::vector<bectl::Snapshot> m_snapshots;
    std::string m_filter;
    model::SortSpec m_sort;
    std::string m_selectedName;
    int m_selectedSnapshot = 0;
    bool m_snapshotsFocused = false;
    int m_tab = InfoTab;
    bool m_filterMode = false;
    bool m_sortChooser = false;
    int m_sortChooserIndex = 0;

    std::vector<std::string> m_log;
    std::string m_status;
    bool m_error = false;
    std::atomic<bool> m_busy{false};
    std::thread m_worker;

    ftxui::ScreenInteractive *m_screen = nullptr;
    HitBoxes m_boxes;
    ftxui::Component m_input;
};

} // namespace tui

#endif
