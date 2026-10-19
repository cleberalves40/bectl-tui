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

// ---------------------------------------------------------------------------
// Operações e diálogos (v0.2.0), com um bectl falso que lista três BEs e
// registra as escritas. Sem elevador: nada de doas nos testes.
// ---------------------------------------------------------------------------

namespace {

struct OpsFixture {
    TempDir dir;
    std::string log = dir.file("ops.log");
    tui::App app;
    explicit OpsFixture(int status = 0)
        : app([this, status] {
              const std::string bectl = dir.file("bectl");
              writeExecutable(bectl,
                              "#!/bin/sh\n"
                              "case \"$*\" in\n"
                              "'list -H -s') printf 'default\\n  zroot/ROOT/default\\tNR\\t/\\t1.2G\\t2026-10-01 10:00\\n"
                              "  zroot/ROOT/default@antes\\t-\\t-\\t300M\\t2026-10-04 09:00\\n"
                              "  zroot/ROOT/default@depois\\t-\\t-\\t1M\\t2026-10-05 09:00\\n' ;;\n"
                              "'list -H') printf 'default\\tNR\\t/\\t1.2G\\t2026-10-01 10:00\\n"
                              "be-mounted\\t-\\t/mnt/be\\t10M\\t2026-10-02 11:00\\n"
                              "be-idle\\t-\\t-\\t5M\\t2026-10-03 12:00\\n' ;;\n"
                              "*) echo \"$*\" >> '" + log + "'\n"
                              "   [ " + std::to_string(status) + " -eq 0 ] || { echo boom >&2; exit "
                                  + std::to_string(status) + "; } ;;\n"
                              "esac\n");
              tui::Options o;
              o.bectlPath = bectl;
              o.iconMode = "ascii";
              o.language = "en_US";
              o.settingsPath = dir.file("config");
              o.elevator = "";
              return o;
          }())
    {
        app.refresh();   // sem tela: roda na hora
    }
    void type(const std::string &s)
    {
        for (char c : s)
            app.handleEvent(Event::Character(c));
    }
    std::vector<std::string> ops() const { return readLines(log); }
    std::string selected() { const auto s = app.viewState(); return s.rows[s.selected].name; }
};

const Event kCtrlN = Event::Special("\x0e");
const Event kCtrlS = Event::Special("\x13");
const Event kCtrlT = Event::Special("\x14");
const Event kCtrlU = Event::Special("\x15");
const Event kAltM = Event::Special("\x1b" "m");

} // namespace

TEST_CASE("Ctrl+N cria BE com origem e -r", "[ops]")
{
    OpsFixture f;
    REQUIRE(f.app.viewState().totalEnvironments == 3);
    f.app.handleEvent(kCtrlN);
    REQUIRE(f.app.dialog().kind == tui::Dialog::Form);
    CHECK(f.app.dialog().title == "Create Boot Environment");
    f.type("novo");
    f.app.handleEvent(Event::Tab);
    f.app.handleEvent(Event::ArrowRight);   // (BE atual) → default
    f.app.handleEvent(Event::Tab);
    f.app.handleEvent(Event::Character(' '));
    f.app.handleEvent(Event::Return);
    CHECK(f.ops() == std::vector<std::string>{"create -r -e default novo"});
    REQUIRE(f.app.dialog().kind == tui::Dialog::Message);
    CHECK(f.app.dialog().message == "Boot Environment 'novo' created.");
    f.app.handleEvent(Event::Return);
    CHECK(f.app.dialog().kind == tui::Dialog::None);
}

TEST_CASE("Esc cancela o formulário; nome vazio é recusado", "[ops]")
{
    OpsFixture f;
    f.app.handleEvent(kCtrlN);
    f.type("x");
    f.app.handleEvent(Event::Escape);
    CHECK(f.app.dialog().kind == tui::Dialog::None);
    f.app.handleEvent(kCtrlN);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.dialog().message == "Please enter a name.");
    CHECK(f.ops().empty());
}

TEST_CASE("F2 renomeia com o nome atual preenchido", "[ops]")
{
    OpsFixture f;
    f.app.handleEvent(Event::ArrowDown);   // be-mounted
    f.app.handleEvent(Event::F2);
    REQUIRE(f.app.dialog().fields.at(0).text == "be-mounted");
    for (int i = 0; i < 7; ++i)
        f.app.handleEvent(Event::Backspace);
    f.type("novo");
    f.app.handleEvent(Event::Return);
    CHECK(f.ops() == std::vector<std::string>{"rename be-mounted be-novo"});
}

TEST_CASE("Enter e Ctrl+T ativam após confirmação", "[ops]")
{
    OpsFixture f;
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);
    REQUIRE(f.app.dialog().kind == tui::Dialog::Confirm);
    CHECK(f.app.dialog().message == "Activate 'be-mounted' permanently?");
    f.app.handleEvent(Event::Return);   // Sim tem o foco
    f.app.handleEvent(kCtrlT);
    f.app.handleEvent(Event::Character('n'));   // não
    f.app.handleEvent(kCtrlT);
    f.app.handleEvent(Event::Character('y'));
    CHECK(f.ops() == std::vector<std::string>{"activate be-mounted", "activate -t be-mounted"});
}

TEST_CASE("Espaço marca; Del destrói os marcados e protege o BE em uso", "[ops]")
{
    OpsFixture f;
    f.app.handleEvent(Event::Character(' '));   // default (em uso)
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Character(' '));   // be-mounted
    CHECK(f.app.viewState().marked == std::set<std::string>{"be-mounted", "default"});
    f.app.handleEvent(Event::Delete);
    REQUIRE(f.app.dialog().kind == tui::Dialog::Confirm);
    CHECK(f.app.dialog().danger);
    CHECK(has(f.app.dialog().message, "Skipped (active):\n• default"));
    CHECK(has(f.app.dialog().message, "'be-mounted'"));
    f.app.handleEvent(Event::Return);   // perigoso: "Não" tem o foco
    CHECK(f.ops().empty());
    f.app.handleEvent(Event::Delete);
    f.app.handleEvent(Event::ArrowLeft);   // → Sim
    f.app.handleEvent(Event::Return);
    CHECK(f.ops() == std::vector<std::string>{"destroy -F be-mounted"});
    REQUIRE(f.app.dialog().kind == tui::Dialog::Message);
    CHECK(has(f.app.dialog().message, "be-mounted"));
    CHECK(f.app.viewState().marked == std::set<std::string>{"default"});
}

TEST_CASE("Del só no BE em uso: avisa e não destrói", "[ops]")
{
    OpsFixture f;
    f.app.handleEvent(Event::Delete);
    CHECK(f.app.dialog().kind == tui::Dialog::Message);
    CHECK(f.ops().empty());
}

TEST_CASE("Alt+M monta só BE não montado; Ctrl+U desmonta", "[ops]")
{
    OpsFixture f;
    f.app.handleEvent(kAltM);   // default já está montado em /
    CHECK(f.app.dialog().kind == tui::Dialog::None);
    f.app.handleEvent(kCtrlU);  // nem se desmonta o BE em uso
    f.app.handleEvent(Event::End);   // be-idle
    f.app.handleEvent(kAltM);
    REQUIRE(f.app.dialog().kind == tui::Dialog::Form);
    f.type("/mnt/x");
    f.app.handleEvent(Event::Return);
    f.app.handleEvent(Event::ArrowUp);   // be-mounted
    f.app.handleEvent(kCtrlU);
    CHECK(f.ops() == std::vector<std::string>{"mount be-idle /mnt/x", "umount be-mounted"});
}

TEST_CASE("Ctrl+S cria snapshot do BE selecionado", "[ops]")
{
    OpsFixture f;
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(kCtrlS);
    REQUIRE(f.app.dialog().fields.at(0).choices.at(f.app.dialog().fields[0].choice) == "be-mounted");
    f.app.handleEvent(Event::Tab);
    f.type("s1");
    f.app.handleEvent(Event::Return);
    CHECK(f.ops() == std::vector<std::string>{"create be-mounted@s1"});
}

TEST_CASE("tabela de snapshots: Enter clona, Del destrói o snapshot", "[ops]")
{
    OpsFixture f;
    f.app.handleEvent(Event::Tab);
    REQUIRE(f.app.viewState().snapshotsFocused);
    f.app.handleEvent(Event::Return);
    REQUIRE(f.app.dialog().kind == tui::Dialog::Form);
    f.type("clone1");
    f.app.handleEvent(Event::Return);
    f.app.handleEvent(Event::ArrowDown);   // default@depois
    f.app.handleEvent(Event::Delete);
    REQUIRE(f.app.dialog().kind == tui::Dialog::Confirm);
    f.app.handleEvent(Event::Character('y'));
    CHECK(f.ops() == std::vector<std::string>{"create -e default@antes clone1", "destroy default@depois"});
}

TEST_CASE("clonar sem snapshot selecionado pede a seleção", "[ops]")
{
    OpsFixture f;
    f.app.handleEvent(Event::F9);
    f.app.handleEvent(Event::ArrowDown);   // Ativar
    f.app.handleEvent(Event::ArrowDown);   // Ativar (temporário)
    f.app.handleEvent(Event::ArrowDown);   // Renomear
    f.app.handleEvent(Event::ArrowDown);   // (separador) → Criar snapshot
    f.app.handleEvent(Event::ArrowDown);   // Clonar snapshot
    f.app.handleEvent(Event::Return);
    CHECK(f.app.dialog().message == "Select a snapshot.");
    CHECK(f.app.viewState().tab == tui::SnapshotsTab);
}

TEST_CASE("menus: F9, Alt+A, Alt+J, separadores e Esc", "[ops]")
{
    OpsFixture f;
    f.app.handleEvent(Event::F9);
    REQUIRE(f.app.dialog().kind == tui::Dialog::Menu);
    CHECK(f.app.dialog().menu == 1);
    f.app.handleEvent(Event::Return);   // Criar…
    CHECK(f.app.dialog().title == "Create Boot Environment");
    f.app.handleEvent(Event::Escape);

    f.app.handleEvent(Event::Special("\x1b" "a"));
    CHECK(f.app.dialog().menu == 0);
    f.app.handleEvent(Event::ArrowDown);   // pula o separador
    CHECK(f.app.dialog().item == 2);
    f.app.handleEvent(Event::Escape);
    CHECK(f.app.dialog().kind == tui::Dialog::None);

    f.app.handleEvent(Event::Special("\x1b" "j"));
    f.app.handleEvent(Event::Return);   // Atalhos de teclado
    CHECK(f.app.viewState().tab == tui::HelpTab);
    f.app.handleEvent(Event::F9);
    f.app.handleEvent(Event::ArrowRight);   // Ajuda
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);   // Sobre
    CHECK(has(f.app.dialog().message, std::string("bectl-tui ") + BECTL_TUI_VERSION));
}

TEST_CASE("falha do bectl vira mensagem de erro e status", "[ops]")
{
    OpsFixture f(1);
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);
    f.app.handleEvent(Event::Return);
    REQUIRE(f.app.dialog().kind == tui::Dialog::Message);
    CHECK(f.app.dialog().title == "Error");
    CHECK(has(f.app.dialog().message, "boom"));
    const tui::ViewState s = f.app.viewState();
    CHECK(s.error);
    CHECK(has(s.status, "ERROR:"));
}

TEST_CASE("diálogos aparecem na tela", "[view]")
{
    OpsFixture f;
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Character(' '));
    f.app.handleEvent(Event::Delete);
    std::string out = draw(f.app.viewState());
    for (const char *t : {"Confirm destroy", "Yes", "No", "be-mounted", "*", "1 marked"})
        CHECK(has(out, t));
    f.app.handleEvent(Event::Escape);
    f.app.handleEvent(kCtrlN);
    f.type("abc");
    out = draw(f.app.viewState());
    for (const char *t : {"Create Boot Environment", "Name:", "abc", "Clone from:", "(Current Boot Environment)",
                          "OK", "Cancel"})
        CHECK(has(out, t));
    f.app.handleEvent(Event::Escape);
    f.app.handleEvent(Event::F9);
    out = draw(f.app.viewState());
    for (const char *t : {"File", "Actions", "Help", "Create snapshot", "Ctrl+S", "Unmount"})
        CHECK(has(out, t));
}
