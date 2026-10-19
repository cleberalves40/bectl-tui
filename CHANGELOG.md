# Changelog

Todas as mudanças relevantes do bectl-tui são registradas aqui.
O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto usa [versionamento semântico](https://semver.org/lang/pt-BR/),
com tags no formato `vX.Y.Z`.

## [0.2.0] - 2026-10-19

Operações e diálogos.

### Adicionado
- Operações do bectl-gui 1.2.0, com os mesmos textos, validações e confirmações:
  criar (**Ctrl+N**: nome, origem — BE atual, outro BE ou snapshot — e `-r`),
  ativar (**Enter**) e ativar temporariamente (**Ctrl+T**), renomear (**F2**, a
  seleção acompanha o novo nome), destruir (**Delete**), montar (**Alt+M**, só BE
  não montado) e desmontar (**Ctrl+U**, nunca o BE em uso), criar snapshot
  (**Ctrl+S**), e clonar (**Enter**) e destruir (**Delete**) o snapshot com a
  tabela de snapshots em foco.
- Marcação com **Espaço** para destruir vários BEs de uma vez; o BE em uso é
  pulado e listado na confirmação ("Ignorados (ativos)"), e o resultado mostra os
  destruídos e as falhas.
- Diálogos de formulário, confirmação (ao destruir, em vermelho e com o foco em
  **Não**) e mensagem, sobrepostos à tela.
- Menus Arquivo, Ações e Ajuda (**F9**, **Alt+A**, **Alt+C**, **Alt+J**), com os
  atalhos de cada item; Ajuda → Sobre mostra a versão.
- Barra de teclas no estilo do mc (F1, F2, F5, F6, F9, F10) e a aba Ajuda com
  todos os atalhos.
- doas/sudo: sem senha a pedir, a operação roda em segundo plano; se o doas/sudo
  pedir senha, a tela é suspensa enquanto ele usa o terminal. Preferência
  `elevator` no arquivo de configuração.
- Erros do `bectl` aparecem num diálogo, no status e na aba Saída.
- Testes: cada operação e diálogo dirigidos por teclas contra um `bectl` falso; no
  FreeBSD, a interface cria e destrói um BE de verdade no pool de rascunho num
  pseudo-terminal; o teste de fumaça abre o diálogo de criação.

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

[0.2.0]: https://github.com/cleberalves40/bectl-tui/releases/tag/v0.2.0
[0.1.0]: https://github.com/cleberalves40/bectl-tui/releases/tag/v0.1.0
