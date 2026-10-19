#ifndef BECTL_TUI_VIEW_H
#define BECTL_TUI_VIEW_H

#include "bectl.h"
#include "icons.h"
#include "listmodel.h"

#include <array>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <set>
#include <string>
#include <vector>

namespace tui {

enum Tab { InfoTab = 0, OutputTab, HelpTab, SnapshotsTab, TabCount };

/** Campo de um formulário: texto, caixa de marcar ou escolha entre opções. */
struct DialogField {
    enum Kind { Text, Check, Choice } kind = Text;
    std::string label;
    std::string text;                    // Text
    bool checked = false;                // Check
    std::vector<std::string> choices;    // Choice
    int choice = 0;
    std::string hint;                    // linha de ajuda abaixo do campo
};

/**
 * Diálogo modal: formulário (criar, renomear, montar, snapshot, clonar),
 * confirmação (Sim/Não), mensagem, ou os menus (F9).
 */
struct Dialog {
    enum Kind { None, Form, Confirm, Message, Menu } kind = None;
    std::string title;
    std::string message;
    std::vector<DialogField> fields;
    // Form: índice do campo, ou fields.size() = OK e fields.size() + 1 =
    // Cancelar. Confirm: 0 = Sim, 1 = Não.
    int focus = 0;
    bool danger = false;                 // confirmação destrutiva (Não por padrão)
    // Menu: títulos, itens (rótulo, atalho) e posição.
    std::vector<std::string> menus;
    std::vector<std::vector<std::pair<std::string, std::string>>> items;
    int menu = 0;
    int item = 0;
};

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
    std::set<std::string> marked;               // BEs marcados para destruir em lote
    Dialog dialog;
};

/** Remove os "&" dos aceleradores de menu do bectl-gui ("&Ações" → "Ações"). */
std::string stripMnemonic(const std::string &label);

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
