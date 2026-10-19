# Changelog

Todas as mudanças relevantes do bectl-tui são registradas aqui.
O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto usa [versionamento semântico](https://semver.org/lang/pt-BR/),
com tags no formato `vX.Y.Z`.

## [0.1.0] - 2026-10-19

Primeira versão: somente leitura.

### Adicionado
- Tela principal em FTXUI com a estrutura do bectl-gui: tabela de Boot
  Environments (estado, nome, ativo, montagem, espaço, criado), campo de filtro,
  abas **Informação**, **Saída**, **Ajuda** e **Snapshots**, barra de status e
  barra de teclas.
- Ícones Nerd Fonts (padrão), Unicode ou ASCII (`--icons`).
- Ordenação por coluna, com o comportamento do bectl-gui 1.2.0: espaço em bytes,
  data cronológica e estado (em uso, próximo boot, inativo); clique no cabeçalho
  (crescente, decrescente, ordem do `bectl`) ou **F6**. A ordem é salva em
  `~/.config/bectl-tui/config`.
- Seleção que acompanha o BE ao filtrar, ordenar e atualizar.
- Listagem em segundo plano: a tela continua respondendo enquanto o `bectl` roda.
- Atalhos: F5/Ctrl+R, Ctrl+F e /, F6, 1–4 e Alt+1–4, F1, Tab, Ctrl+Q, F10 e q;
  mouse no cabeçalho, nas linhas e nas abas (veja `docs/KEYMAP.md`).
- Idiomas en_US, pt_BR e es_ES, com os textos do bectl-gui (`--lang`).
- Núcleo em C++17 sem dependências: porte do `BectlManager` do bectl-gui 1.2.0,
  com as mesmas regras de validação e a mesma leitura de `bectl list` e
  `bectl list -s`.
- Testes: núcleo (porte do `tst_validation`), modelo, tela e integração contra o
  `bectl(8)` real (porte do `tst_bectlmanager`), mais a interface num
  pseudo-terminal (`tests/tui-smoke.sh` e `tests/freebsd-e2e.sh`); CI no Ubuntu
  (FTXUI 6.1.9, ASan e UBSan) e no FreeBSD 14.5 e 15.1, em amd64 e aarch64.

[0.1.0]: https://github.com/cleberalves40/bectl-tui/releases/tag/v0.1.0
