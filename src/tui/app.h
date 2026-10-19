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
#include <functional>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace tui {

struct Options {
    std::string bectlPath;       // vazio: bectl(8) do PATH
    std::string language;        // vazio: preferência salva ou ambiente
    std::string iconMode;        // vazio: preferência salva ou "nerd"
    std::string settingsPath;    // vazio: ~/.config/bectl-tui/config
    std::string elevator = "-";  // "-": preferência salva, ou doas (vazio se root)
};

class App
{
public:
    explicit App(const Options &options);
    ~App();

    /** Laço da interface em tela cheia; devolve o código de saída. */
    int run();

    // Para os testes: o estado que a tela mostraria e o tratamento de eventos.
    // Sem tela (run() não chamado), listagens e operações rodam na hora.
    ViewState viewState();
    bool handleEvent(const ftxui::Event &event);
    void applyListing(const std::vector<bectl::BootEnvironment> &list,
                      const std::vector<bectl::Snapshot> &snapshots, const std::string &error);
    void refresh();
    bool filterMode() const { return m_filterMode; }
    std::string &filterText() { return m_filter; }
    const bectl::Settings &settings() const { return m_settings; }
    const Dialog &dialog() const { return m_dialog; }

private:
    using Op = std::function<void()>;
    using FormOp = std::function<void(const std::vector<DialogField> &)>;

    // Ações (as mesmas do bectl-gui 1.2.0).
    void actCreate();
    void actActivate(bool temporary);
    void actRename();
    void actDestroy();
    void actMount();
    void actUnmount();
    void actSnapshot();
    void actCloneSnapshot();
    void actDestroySnapshot();
    void actAbout();
    void openMenu(int menu);

    /**
     * Executa op (uma chamada ao bectl). Sem senha a pedir, roda em segundo
     * plano; se o doas/sudo precisar da senha, devolve o terminal enquanto
     * isso. Depois, onSuccess (na thread da interface) e uma nova listagem.
     */
    void runOperation(const std::string &status, const std::string &command, const Op &op,
                      const Op &onSuccess);
    void finishOperation(const std::string &error, const Op &onSuccess);
    bool passwordNeeded();

    void showMessage(const std::string &title, const std::string &message);
    void showConfirm(const std::string &title, const std::string &message, bool danger, const Op &onYes);
    void showForm(const std::string &title, std::vector<DialogField> fields, FormOp onAccept);
    bool handleDialogEvent(const ftxui::Event &event);
    bool refuseWhileBusy();

    std::string currentName();
    const bectl::BootEnvironment *current();
    std::string currentSnapshot();
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
    bool m_started = false;   // primeira listagem já disparada
    bool m_sortChooser = false;
    int m_sortChooserIndex = 0;
    std::set<std::string> m_marked;

    Dialog m_dialog;
    Op m_onAccept;                                 // Confirm
    FormOp m_onForm;                               // Form
    std::vector<std::vector<Op>> m_menuActions;    // Menu

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
