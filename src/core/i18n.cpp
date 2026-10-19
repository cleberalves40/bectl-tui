#include "i18n.h"

#include <cstdlib>
#include <map>

namespace i18n {

namespace {

std::string g_locale = "en_US";

using Catalog = std::map<std::string, std::string>;

// { inglês (chave), pt_BR, es_ES }
struct Entry {
    const char *en;
    const char *pt;
    const char *es;
};

const Entry kEntries[] = {
    // Mensagens do núcleo (mesmos textos do bectl-gui)
    {"Failed to start '%1': %2", "Falha ao iniciar '%1': %2", "No se pudo iniciar '%1': %2"},
    {"Command timed out (%1 ms): %2", "Comando excedeu o tempo limite (%1 ms): %2",
     "El comando excedió el tiempo límite (%1 ms): %2"},
    {"Process crashed: %1", "Processo terminou com crash: %1", "El proceso terminó abruptamente: %1"},
    {"Command failed with exit code %1", "Comando falhou com código %1", "El comando falló con código %1"},
    {"Snapshot name must contain '@' (e.g. default@before-upgrade)",
     "O nome do snapshot deve conter '@' (ex: default@antes-upgrade)",
     "El nombre del snapshot debe contener '@' (ej: default@antes-actualizacion)"},
    {"Invalid boot environment name: %1", "Nome de Boot Environment inválido: %1",
     "Nombre de Boot Environment no válido: %1"},
    {"Invalid snapshot name: %1", "Nome de snapshot inválido: %1", "Nombre de snapshot no válido: %1"},
    {"Mount point must be an absolute path: %1", "O ponto de montagem deve ser um caminho absoluto: %1",
     "El punto de montaje debe ser una ruta absoluta: %1"},
    {"%1: cannot destroy the currently active BE", "%1: não é possível destruir o BE ativo no momento",
     "%1: no se puede destruir el BE activo en este momento"},
    {"'%1' not found. Install it (pkg install %1) or run bectl-tui as root.",
     "'%1' não encontrado. Instale-o (pkg install %1) ou execute o bectl-tui como root.",
     "'%1' no encontrado. Instálelo (pkg install %1) o ejecute bectl-tui como root."},

    // Interface
    {"Boot Environment", "Boot Environment", "Boot Environment"},
    {"Active", "Ativo", "Activo"},
    {"Mountpoint", "Montagem", "Montaje"},
    {"Space", "Espaço", "Espacio"},
    {"Created", "Criado", "Creado"},
    {"Information", "Informação", "Información"},
    {"Output", "Saída", "Salida"},
    {"Help", "Ajuda", "Ayuda"},
    {"Snapshots", "Snapshots", "Snapshots"},
    {"Snapshot", "Snapshot", "Snapshot"},
    {"Filter by name…", "Filtrar por nome…", "Filtrar por nombre…"},
    {"Ready.", "Pronto.", "Listo."},
    {"Listing Boot Environments…", "Listando Boot Environments…", "Listando Boot Environments…"},
    {"List updated (%1 BE(s)).", "Lista atualizada (%1 BE(s)).", "Lista actualizada (%1 BE(s))."},
    {"%1 BE(s)", "%1 BE(s)", "%1 BE(s)"},
    {"ERROR: %1", "ERRO: %1", "ERROR: %1"},
    {"Error", "Erro", "Error"},
    {"Theme: %1", "Tema: %1", "Tema: %1"},
    {"System", "Sistema", "Sistema"},
    {"State", "Estado", "Estado"},
    {"Active now", "Ativo agora", "Activo ahora"},
    {"Active on next boot", "Ativo no próximo boot", "Activo en el próximo arranque"},
    {"Active on next boot only (temporary)", "Ativo só no próximo boot (temporário)",
     "Activo solo en el próximo arranque (temporal)"},
    {"Inactive", "Inativo", "Inactivo"},
    {"Select a Boot Environment to see details.", "Selecione um Boot Environment para ver os detalhes.",
     "Seleccione un Boot Environment para ver los detalles."},
    {"No snapshots.", "Nenhum snapshot.", "Ningún snapshot."},
    {"No Boot Environments.", "Nenhum Boot Environment.", "Ningún Boot Environment."},
    {"Sort by", "Ordenar por", "Ordenar por"},
    {"(bectl order)", "(ordem do bectl)", "(orden de bectl)"},
    {"State icon", "Ícone de estado", "Icono de estado"},
    {"Wait for the current operation to finish.", "Aguarde o fim da operação em andamento.",
     "Espere a que termine la operación en curso."},
    {"Refresh", "Atualizar", "Actualizar"},
    {"Sort", "Ordenar", "Ordenar"},
    {"Filter", "Filtrar", "Filtrar"},
    {"Tabs", "Abas", "Pestañas"},
    {"Quit", "Sair", "Salir"},
    {"Move", "Mover", "Mover"},
    {"Keyboard shortcuts", "Atalhos de teclado", "Atajos de teclado"},
    {"Focus filter", "Ir para o filtro", "Ir al filtro"},
    {"Leave filter", "Sair do filtro", "Salir del filtro"},
    {"Switch the focused table", "Alternar a tabela com foco", "Alternar la tabla con foco"},
    {"Choose the sort column (again: reverse; (bectl order): original order)",
     "Escolher a coluna de ordenação (de novo: inverter; (ordem do bectl): ordem original)",
     "Elegir la columna de orden (otra vez: invertir; (orden de bectl): orden original)"},
    {"Click a column header to sort", "Clique no cabeçalho de uma coluna para ordenar",
     "Haga clic en el encabezado de una columna para ordenar"},
    {"This version is read-only: operations arrive in v0.2.0.",
     "Esta versão é só de leitura: as operações chegam na v0.2.0.",
     "Esta versión es de solo lectura: las operaciones llegan en la v0.2.0."},
};

const Catalog &catalog(const std::string &loc)
{
    static const Catalog pt = [] {
        Catalog c;
        for (const auto &e : kEntries)
            c[e.en] = e.pt;
        return c;
    }();
    static const Catalog es = [] {
        Catalog c;
        for (const auto &e : kEntries)
            c[e.en] = e.es;
        return c;
    }();
    static const Catalog none;
    if (loc == "pt_BR")
        return pt;
    if (loc == "es_ES")
        return es;
    return none;
}

} // namespace

void initFromEnvironment()
{
    for (const char *var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char *v = std::getenv(var);
        if (!v || !*v)
            continue;
        const std::string value(v);
        if (value.rfind("pt", 0) == 0)
            setLocale("pt_BR");
        else if (value.rfind("es", 0) == 0)
            setLocale("es_ES");
        else
            setLocale("en_US");
        return;
    }
    setLocale("en_US");
}

void setLocale(const std::string &loc)
{
    g_locale = (loc == "pt_BR" || loc == "es_ES") ? loc : "en_US";
}

std::string locale()
{
    return g_locale;
}

std::vector<std::string> availableLocales()
{
    return {"en_US", "pt_BR", "es_ES"};
}

std::string tr(const char *key)
{
    const Catalog &c = catalog(g_locale);
    const auto it = c.find(key);
    return it == c.end() ? std::string(key) : it->second;
}

std::string trf(const char *key, std::initializer_list<std::string> args)
{
    const std::string pattern = tr(key);
    const std::vector<std::string> values(args);
    std::string out;
    out.reserve(pattern.size());
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '%' && i + 1 < pattern.size() && pattern[i + 1] >= '1' && pattern[i + 1] <= '9') {
            const size_t n = static_cast<size_t>(pattern[i + 1] - '1');
            if (n < values.size()) {
                out += values[n];
                ++i;
                continue;
            }
        }
        out += pattern[i];
    }
    return out;
}

} // namespace i18n
