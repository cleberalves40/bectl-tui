# Atalhos: bectl-gui → bectl-tui

O bectl-tui mantém os atalhos do bectl-gui 1.2.0 sempre que o terminal os entrega ao
programa. Alguns não chegam, ou chegam iguais a outra tecla, e ganham uma alternativa:

- o terminal envia <kbd>Ctrl+M</kbd> como <kbd>Enter</kbd> e <kbd>Ctrl+I</kbd> como
  <kbd>Tab</kbd>;
- <kbd>Ctrl+Shift+letra</kbd> chega igual a <kbd>Ctrl+letra</kbd>;
- <kbd>Ctrl+Enter</kbd>, <kbd>Ctrl+1</kbd>…<kbd>Ctrl+4</kbd> e <kbd>Ctrl+,</kbd> não
  são enviados pela maioria dos terminais.

| Ação | bectl-gui | bectl-tui | Disponível |
|:---|:---|:---|:---|
| Atualizar | F5 / Ctrl+R | **F5 / Ctrl+R** | v0.1.0 |
| Filtrar | Ctrl+F | **Ctrl+F** ou **/** | v0.1.0 |
| Ordenar | clique no cabeçalho | clique no cabeçalho ou **F6** (escolher a coluna, como no htop) | v0.1.0 |
| Abas Informação / Saída / Ajuda / Snapshots | Ctrl+1 … Ctrl+4 | **1 … 4** ou **Alt+1 … Alt+4**; **F1** = Ajuda | v0.1.0 |
| Alternar a tabela com foco | — | **Tab** | v0.1.0 |
| Sair | Ctrl+Q | **Ctrl+Q**, **F10** ou **q** | v0.1.0 |
| Criar | Ctrl+N | **Ctrl+N** | v0.2.0 |
| Ativar | Ctrl+Enter | **Enter** (com confirmação) | v0.2.0 |
| Ativar temporariamente | Ctrl+T | **Ctrl+T** | v0.2.0 |
| Renomear | F2 | **F2** | v0.2.0 |
| Criar snapshot | Ctrl+Shift+S | **Ctrl+S** | v0.2.0 |
| Montar | Ctrl+M | **Alt+M** | v0.2.0 |
| Desmontar | Ctrl+U | **Ctrl+U** | v0.2.0 |
| Destruir | Delete | **Delete**; com a tabela de snapshots em foco, destrói o snapshot | v0.2.0 |
| Menus | barra de menus | **F9** ou **Alt+A** (Arquivo), **Alt+E** (Editar), **Alt+C** (Ações), **Alt+J** (Ajuda) | v0.2.0 |
| Preferências | Ctrl+, | **Alt+,** | v0.3.0 |
| Próximo tema | Ctrl+Shift+T | **Alt+T** | v0.3.0 |

O FTXUI coloca o terminal em modo cru, então <kbd>Ctrl+S</kbd> e <kbd>Ctrl+Q</kbd> chegam
ao programa e não pausam nem retomam o terminal (controle de fluxo XON/XOFF).
