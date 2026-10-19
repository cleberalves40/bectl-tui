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
 F1 Ajuda  F2 Renomear  F5 Atualizar  F6 Ordenar  F9 Menu  F10 Sair
```

## Estado

**v0.2.0: operações.** Além de listar, filtrar e ordenar, cria, ativa (também
temporariamente), renomeia, destrói (vários de uma vez, protegendo o BE em uso), monta e
desmonta BEs, e cria, clona e destrói snapshots, com os diálogos, confirmações e menus
do bectl-gui. Preferências, temas e idiomas chegam na v0.3.0, e a v1.0.0 terá paridade
completa com o bectl-gui 1.2.0. Veja o [CHANGELOG](CHANGELOG.md).

### Senha (doas/sudo)

Como usuário comum, as escritas passam pelo `doas` (padrão; `elevator=sudo` em
`~/.config/bectl-tui/config` troca para o `sudo`, e `elevator=` executa o `bectl`
direto). Se o `doas` não pedir senha (regra `nopass` no `doas.conf`), a operação roda
em segundo plano. Se pedir, a tela é suspensa, o terminal mostra o comando e a pergunta
da senha, e a interface volta em seguida. Como root, o `bectl` roda direto.

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
tabela e o `elevator` (doas, sudo ou vazio).

## Teclado e mouse (v0.2.0)

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
| <kbd>Ctrl+N</kbd> | Criar Boot Environment |
| <kbd>Enter</kbd> / <kbd>Ctrl+T</kbd> | Ativar / ativar temporariamente (com confirmação) |
| <kbd>F2</kbd> | Renomear |
| <kbd>Espaço</kbd> | Marcar para destruir vários de uma vez |
| <kbd>Delete</kbd> | Destruir os marcados, ou o selecionado |
| <kbd>Alt+M</kbd> / <kbd>Ctrl+U</kbd> | Montar / desmontar |
| <kbd>Ctrl+S</kbd> | Criar snapshot |
| <kbd>Enter</kbd> / <kbd>Delete</kbd> na tabela de snapshots | Clonar / destruir o snapshot |
| <kbd>F9</kbd>, <kbd>Alt+A</kbd>, <kbd>Alt+C</kbd>, <kbd>Alt+J</kbd> | Menus Ações, Arquivo, Ações, Ajuda |

O mapa completo, com a correspondência para os atalhos do bectl-gui, está em
[docs/KEYMAP.md](docs/KEYMAP.md).

## Arquitetura

| Camada | Local | Responsabilidade |
|:---|:---|:---|
| Núcleo | `src/core/` | `bectl(8)` sem shell, validação de nomes (regras do `libbe`), leitura de `bectl list` e `list -s`, tamanhos, i18n e preferências. C++17 puro: um porte do `BectlManager` do bectl-gui 1.2.0. |
| Modelo | `src/model/` | Filtro, ordenação (espaço em bytes, data, estado) e seleção que acompanha o BE |
| Interface | `src/tui/` | FTXUI: tela, diálogos, menus, teclado e mouse; listagem e operações em segundo plano, e o terminal devolvido ao doas/sudo quando há senha |

## Testes

```sh
cmake -B build -S . -DBECTL_TUI_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
sh tests/freebsd-e2e.sh build    # no FreeBSD, como root: integração com o bectl real
```

- `test_core`: porte do `tst_validation` do bectl-gui (os mesmos casos), mais a
  execução de processos e as preferências.
- `test_model`: filtro, ordenação e seleção.
- `test_view`: desenha a tela em texto e dirige a interface com eventos de teclado,
  incluindo cada operação e diálogo contra um `bectl` falso que registra os comandos.
- `test_integration`: porte do `tst_bectlmanager`, contra o `bectl(8)` real num pool
  ZFS de rascunho, sem tocar nos BEs do sistema.

O CI roda no Ubuntu (com ASan e UBSan) e no FreeBSD 14.5 e 15.1, em amd64 e aarch64,
incluindo a interface num pseudo-terminal: no FreeBSD, ela cria e destrói um BE de
verdade no pool de rascunho pelos diálogos.

## Licença

[MIT](LICENSE).
