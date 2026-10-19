// Tela: renderiza a interface em texto e confere o que aparece, e dirige a
// App com eventos de teclado, sem terminal.

#include "app.h"
#include "i18n.h"
#include "support.h"
#include "view.h"

#include <catch2/catch_test_macros.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

using namespace ftxui;

namespace {

std::vector<bectl::BootEnvironment> sample()
{
    return {
        {"default", "NR", "/", "1.2G", "2026-10-01 10:00"},
        {"be-mounted", "-", "/mnt/be", "10M", "2026-10-02 11:00"},
        {"be-idle", "-", "-", "5M", "2026-10-03 12:00"},
    };
}

std::vector<bectl::Snapshot> sampleSnapshots()
{
    return {
        {"default@antes", "300M", "2026-10-04 09:00"},
        {"default@depois", "1M", "2026-10-05 09:00"},
        {"be-mounted@x", "1K", "2026-10-06 09:00"},
    };
}

std::string draw(const tui::ViewState &s, int width = 120, int height = 34)
{
    auto screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
    Render(screen, tui::render(s, text("")));
    return screen.ToString();
}

bool has(const std::string &haystack, const std::string &needle)
{
    return haystack.find(needle) != std::string::npos;
}

// Posição (linha) do texto na tela desenhada, para comparar ordem.
size_t at(const std::string &screen, const std::string &needle)
{
    return screen.find(needle);
}

struct AppFixture {
    TempDir dir;
    tui::App app;
    AppFixture()
        : app([this] {
              tui::Options o;
              o.bectlPath = dir.file("bectl-nao-usado");
              o.iconMode = "ascii";
              o.language = "en_US";
              o.settingsPath = dir.file("config");
              return o;
          }())
    {
        app.applyListing(sample(), sampleSnapshots(), "");
    }
};

} // namespace

TEST_CASE("tela mostra colunas, linhas e estado", "[view]")
{
    i18n::setLocale("en_US");
    tui::ViewState s;
    s.rows = sample();
    s.selected = 0;
    s.status = "Ready.";
    s.totalEnvironments = 3;
    s.themeLabel = "System";
    s.iconMode = tui::IconMode::Ascii;
    const std::string out = draw(s);
    for (const char *t : {"Boot Environment", "Active", "Mountpoint", "Space", "Created", "default",
                          "be-mounted", "be-idle", "1.2G", "2026-10-03 12:00", "Ready.", "3 BE(s)",
                          "Theme: System", "Information", "Output", "Help", "Snapshots", "F5", "F10"})
        CHECK(has(out, t));
    // Aba Informação do BE selecionado.
    CHECK(has(out, "Active now"));
}

TEST_CASE("indicador de ordenação na coluna e ordem das linhas", "[view]")
{
    i18n::setLocale("en_US");
    tui::ViewState s;
    s.rows = model::visibleRows(sample(), "", {model::Space, true});
    s.sort = {model::Space, true};
    s.iconMode = tui::IconMode::Ascii;
    const std::string out = draw(s);
    CHECK(has(out, "Space ^"));
    CHECK(at(out, "be-idle") < at(out, "be-mounted"));
    CHECK(at(out, "be-mounted") < at(out, "default "));
}

TEST_CASE("ícones Nerd Font no modo padrão", "[view]")
{
    tui::ViewState s;
    s.rows = sample();
    s.selected = 0;
    s.iconMode = tui::IconMode::Nerd;
    const std::string out = draw(s);
    CHECK(has(out, ""));   // nf-fa-check_circle: BE em uso
    CHECK(has(out, ""));   // nf-fa-hdd_o: montado
    CHECK(has(out, ""));   // nf-fa-camera: aba Snapshots
}

TEST_CASE("aba Snapshots lista os do BE e a contagem", "[view]")
{
    i18n::setLocale("en_US");
    tui::ViewState s;
    s.rows = sample();
    s.selected = 0;
    s.snapshots = model::snapshotsOf(sampleSnapshots(), "default");
    s.tab = tui::SnapshotsTab;
    s.iconMode = tui::IconMode::Ascii;
    const std::string out = draw(s);
    CHECK(has(out, "Snapshots (2)"));
    CHECK(has(out, "default@antes"));
    CHECK(has(out, "300M"));
    CHECK_FALSE(has(out, "be-mounted@x"));
}

TEST_CASE("tela em português", "[view]")
{
    i18n::setLocale("pt_BR");
    tui::ViewState s;
    s.rows = sample();
    s.selected = 1;
    s.iconMode = tui::IconMode::Ascii;
    s.themeLabel = i18n::tr("System");
    const std::string out = draw(s);
    for (const char *t : {"Montagem", "Espaço", "Criado", "Informação", "Saída", "Ajuda", "Tema: Sistema",
                          "Inativo"})
        CHECK(has(out, t));
    i18n::setLocale("en_US");
}

TEST_CASE("F6 abre a escolha da coluna e Enter ordena, e a ordem é salva", "[app]")
{
    AppFixture f;
    REQUIRE(f.app.handleEvent(Event::F6));
    CHECK(f.app.viewState().sortChooserOpen);
    for (int i = 0; i < 5; ++i)   // (ordem do bectl) → … → Espaço
        f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);
    tui::ViewState s = f.app.viewState();
    CHECK_FALSE(s.sortChooserOpen);
    CHECK(s.sort == model::SortSpec{model::Space, true});
    CHECK(s.rows.front().name == "be-idle");
    CHECK(f.app.settings().intValue("sort.column", -1) == model::Space);

    // De novo na mesma coluna: inverte.
    f.app.handleEvent(Event::F6);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().sort == model::SortSpec{model::Space, false});
    // "(ordem do bectl)": volta à ordem original.
    f.app.handleEvent(Event::F6);
    f.app.handleEvent(Event::Home);   // Home não move no seletor; sobe até o topo
    for (int i = 0; i < 8; ++i)
        f.app.handleEvent(Event::ArrowUp);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().rows.front().name == "default");
}

TEST_CASE("seleção sobrevive à ordenação e ao filtro", "[app]")
{
    AppFixture f;
    f.app.handleEvent(Event::ArrowDown);   // be-mounted
    CHECK(f.app.viewState().rows[f.app.viewState().selected].name == "be-mounted");
    f.app.handleEvent(Event::F6);
    for (int i = 0; i < 5; ++i)
        f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);
    const tui::ViewState s = f.app.viewState();
    CHECK(s.rows[s.selected].name == "be-mounted");
    f.app.filterText() = "idle";       // be-mounted some: seleciona a primeira
    const tui::ViewState t = f.app.viewState();
    CHECK(t.rows[t.selected].name == "be-idle");
}

TEST_CASE("atalhos de filtro, abas e foco", "[app]")
{
    AppFixture f;
    CHECK(f.app.handleEvent(Event::Character('/')));
    CHECK(f.app.filterMode());
    CHECK_FALSE(f.app.handleEvent(Event::Character('x')));   // vai para o campo de filtro
    f.app.handleEvent(Event::Escape);
    CHECK_FALSE(f.app.filterMode());
    f.app.handleEvent(Event::Special("\x06"));                 // Ctrl+F
    CHECK(f.app.filterMode());
    f.app.handleEvent(Event::Return);

    f.app.handleEvent(Event::Character('2'));
    CHECK(f.app.viewState().tab == tui::OutputTab);
    f.app.handleEvent(Event::Special("\x1b" "3"));             // Alt+3
    CHECK(f.app.viewState().tab == tui::HelpTab);
    f.app.handleEvent(Event::F1);
    CHECK(f.app.viewState().tab == tui::HelpTab);

    // Tab: aba Snapshots com foco na tabela de snapshots do BE selecionado.
    f.app.handleEvent(Event::Tab);
    tui::ViewState s = f.app.viewState();
    CHECK(s.tab == tui::SnapshotsTab);
    CHECK(s.snapshotsFocused);
    CHECK(s.snapshots.size() == 2);
    f.app.handleEvent(Event::ArrowDown);
    CHECK(f.app.viewState().selectedSnapshot == 1);
    f.app.handleEvent(Event::Tab);
    CHECK_FALSE(f.app.viewState().snapshotsFocused);
}

TEST_CASE("listagem com erro aparece no status e na Saída", "[app]")
{
    AppFixture f;
    f.app.applyListing({}, {}, "boom");
    const tui::ViewState s = f.app.viewState();
    CHECK(s.error);
    CHECK(s.status == "ERROR: boom");
    CHECK(s.log.back().find("ERROR: boom") != std::string::npos);
    CHECK(s.totalEnvironments == 3);   // a última listagem boa continua na tela
}
