#ifndef BECTL_TUI_LISTMODEL_H
#define BECTL_TUI_LISTMODEL_H

#include "bectl.h"

#include <string>
#include <vector>

/**
 * Filtro, ordenação e seleção da lista, independentes da interface: o mesmo
 * comportamento do bectl-gui 1.2.0, testável sem desenhar a tela.
 */
namespace model {

enum Column { StateIcon = 0, Name, Active, Mountpoint, Space, Created, ColumnCount };

struct SortSpec {
    int column = -1;         // -1: ordem do bectl
    bool ascending = true;

    bool operator==(const SortSpec &o) const { return column == o.column && ascending == o.ascending; }
};

/** Clique no cabeçalho, como na GUI: crescente, decrescente e sem ordem. */
SortSpec nextOnHeaderClick(SortSpec current, int column);

/** Filtra pelo nome (sem diferenciar maiúsculas) e ordena de forma estável. */
std::vector<bectl::BootEnvironment> visibleRows(const std::vector<bectl::BootEnvironment> &all,
                                                const std::string &filter, SortSpec sort);

/** Ordena snapshots pela coluna (0 nome, 1 espaço, 2 data); column -1 mantém a ordem. */
std::vector<bectl::Snapshot> sortedSnapshots(std::vector<bectl::Snapshot> snaps, SortSpec sort);

/** Snapshots do BE indicado; de todos, se be estiver vazio. */
std::vector<bectl::Snapshot> snapshotsOf(const std::vector<bectl::Snapshot> &all, const std::string &be);

/**
 * Linha a selecionar após atualizar, filtrar ou ordenar: a do BE que estava
 * selecionado, se ainda visível; senão a primeira (ou -1, lista vazia).
 */
int rowToSelect(const std::vector<bectl::BootEnvironment> &rows, const std::string &previous);

} // namespace model

#endif
