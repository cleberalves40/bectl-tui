# bectl-tui

[![Build & Test](https://github.com/cleberalves40/bectl-tui/actions/workflows/build.yml/badge.svg)](https://github.com/cleberalves40/bectl-tui/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Interface de terminal (TUI) para o [`bectl(8)`](https://man.freebsd.org/cgi/man.cgi?bectl(8))
do FreeBSD, para gerenciar Boot Environments ZFS. É a contraparte de terminal do
[bectl-gui](https://github.com/cleberalves40/bectl-gui), com os mesmos contextos e
atalhos sempre que o terminal permite, e ícones [Nerd Fonts](https://www.nerdfonts.com/).

```
 bectl — Boot Environment Manager                         Filtrar por nome…
──────────────────────────────────────────────────────────────────────────────
    Boot Environment              Ativo  Montagem          Espaço  Criado
    default                       NR      /                  1.2G  2026-10-01 10:00
    13.2-antes-upgrade            -      -                   300M  2026-10-05 09:30
    teste                         -       /mnt/be             10M  2026-10-06 11:00
──────────────────────────────────────────────────────────────────────────────
  1  Informação   2  Saída   3  Ajuda   4  Snapshots (2)
 default
 Estado: Ativo agora
──────────────────────────────────────────────────────────────────────────────
 Pronto.                                                 Tema: Sistema  3 BE(s)
 F5 Atualizar  F6 Ordenar  ^F Filtrar  1-4 Abas  F10 Sair
```

## Estado

**v0.1.0: somente leitura.** Lista, filtra e ordena os BEs, mostra detalhes,
snapshots, saída e ajuda. As operações (criar, ativar, renomear, destruir, montar,
snapshots) chegam na v0.2.0, e Preferências, temas e idiomas na v0.3.0. A v1.0.0 terá
paridade completa com o bectl-gui 1.2.0. Veja o [CHANGELOG](CHANGELOG.md).

## Requisitos (FreeBSD)

```sh
pkg install ftxui cmake ninja
pkg install nerd-fonts-jetbrainsmono      # ícones; opcional
```

Se o `pkg` não encontrar o `ftxui` (o ramo `latest` do FreeBSD 15/aarch64 ficou sem ele em
outubro de 2026), instale do ramo `quarterly`: veja `ci/freebsd-ci.sh`.

Para os ícones, configure o terminal com a fonte **JetBrainsMono Nerd Font Mono**. A
variante *Mono* mantém cada ícone com a largura de um caractere, e as colunas ficam
alinhadas. Sem uma fonte Nerd, use `--icons=unicode` ou `--icons=ascii`.

Ao contrário do bectl-gui, o bectl-tui não depende do Qt: o FTXUI é um único pacote,
sem X11 nem Wayland. Ele serve, por exemplo, a um servidor acessado por SSH.

## Compilação

```sh
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bectl-tui
```

Opções: `--icons=nerd|unicode|ascii`, `--lang=en_US|pt_BR|es_ES`, `--config=ARQUIVO`,
`--version`. As preferências ficam em `~/.config/bectl-tui/config`; hoje, a ordem da
tabela.

## Teclado e mouse (v0.1.0)

| Tecla | Ação |
|:---|:---|
| <kbd>F5</kbd> / <kbd>Ctrl+R</kbd> | Atualizar a lista |
| <kbd>↑</kbd> <kbd>↓</kbd> <kbd>PgUp</kbd> <kbd>PgDn</kbd> <kbd>Home</kbd> <kbd>End</kbd> (ou <kbd>j</kbd> <kbd>k</kbd>) | Mover a seleção |
| <kbd>Ctrl+F</kbd> / <kbd>/</kbd> | Filtrar por nome (<kbd>Esc</kbd> ou <kbd>Enter</kbd> volta à tabela) |
| <kbd>F6</kbd> | Escolher a coluna de ordenação: de novo na mesma coluna inverte, e "(ordem do bectl)" volta à ordem original |
| Clique no cabeçalho | Ordenar: crescente, decrescente, ordem do bectl |
| <kbd>1</kbd>–<kbd>4</kbd> / <kbd>Alt+1</kbd>–<kbd>Alt+4</kbd> | Abas Informação, Saída, Ajuda, Snapshots (<kbd>F1</kbd>: Ajuda) |
| <kbd>Tab</kbd> | Alternar o foco entre a tabela de BEs e a de snapshots |
| <kbd>Ctrl+Q</kbd> / <kbd>F10</kbd> / <kbd>q</kbd> | Sair |

O mapa completo, com a correspondência para os atalhos do bectl-gui, está em
[docs/KEYMAP.md](docs/KEYMAP.md).

## Arquitetura

| Camada | Local | Responsabilidade |
|:---|:---|:---|
| Núcleo | `src/core/` | `bectl(8)` sem shell, validação de nomes (regras do `libbe`), leitura de `bectl list` e `list -s`, tamanhos, i18n e preferências. C++17 puro: um porte do `BectlManager` do bectl-gui 1.2.0. |
| Modelo | `src/model/` | Filtro, ordenação (espaço em bytes, data, estado) e seleção que acompanha o BE |
| Interface | `src/tui/` | FTXUI: tela, teclado, mouse e listagem em segundo plano |

## Testes

```sh
cmake -B build -S . -DBECTL_TUI_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
sh tests/freebsd-e2e.sh build    # no FreeBSD, como root: integração com o bectl real
```

- `test_core`: porte do `tst_validation` do bectl-gui (os mesmos casos), mais a
  execução de processos e as preferências.
- `test_model`: filtro, ordenação e seleção.
- `test_view`: desenha a tela em texto e dirige a interface com eventos de teclado.
- `test_integration`: porte do `tst_bectlmanager`, contra o `bectl(8)` real num pool
  ZFS de rascunho, sem tocar nos BEs do sistema.

O CI roda no Ubuntu (com ASan e UBSan) e no FreeBSD 14.5 e 15.1, em amd64 e aarch64,
incluindo a interface num pseudo-terminal.

## Licença

[MIT](LICENSE).
