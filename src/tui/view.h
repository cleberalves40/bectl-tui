#ifndef BECTL_TUI_VIEW_H
#define BECTL_TUI_VIEW_H

#include "bectl.h"
#include "icons.h"
#include "listmodel.h"

#include <array>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <string>
#include <vector>

namespace tui {

enum Tab { InfoTab = 0, OutputTab, HelpTab, SnapshotsTab, TabCount };

/** Tudo o que a tela mostra; montado pela App e desenhado por render(). */
struct ViewState {
    std::vector<bectl::BootEnvironment> rows;   // já filtradas e ordenadas
    int selected = -1;
    model::SortSpec sort;
    std::vector<bectl::Snapshot> snapshots;      // do BE selecionado (ou de todos)
    int selectedSnapshot = -1;
    bool snapshotsFocused = false;
    int tab = InfoTab;
    std::vector<std::string> log;               // aba Saída, com horário
    std::string status;
    bool busy = false;
    bool error = false;
    int totalEnvironments = 0;
    std::string themeLabel;
    IconMode iconMode = IconMode::Nerd;
    bool sortChooserOpen = false;
    int sortChooserIndex = 0;                   // 0 = ordem do bectl, 1… = colunas
};

/** Posições na tela, para o mouse: cabeçalho e linhas da tabela. */
struct HitBoxes {
    std::array<ftxui::Box, model::ColumnCount> header{};
    std::vector<ftxui::Box> rows;
    std::array<ftxui::Box, TabCount> tabs{};
};

/** Rótulo da coluna (com ícone de ordenação, se ordenada). */
std::string columnTitle(int column);

/** Tela completa. filter é o campo de filtro (componente Input ou texto). */
ftxui::Element render(const ViewState &state, ftxui::Element filter, HitBoxes *boxes = nullptr);

} // namespace tui

#endif
