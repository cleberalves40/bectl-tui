// Núcleo: porte do tst_validation do bectl-gui 1.2.0 (mesmos casos), mais a
// execução de processos e as preferências. Roda em qualquer sistema.

#include "bectl.h"
#include "i18n.h"
#include "process.h"
#include "settings.h"
#include "support.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <functional>
#include <stdexcept>

namespace {

struct Fixture {
    TempDir dir;
    std::string log = dir.file("calls.log");

    Fixture() { i18n::setLocale("en_US"); }

    std::vector<std::string> calls() const { return readLines(log); }

    // Executa op esperando exceção e sem que o bectl seja chamado.
    std::string rejected(const std::function<void()> &op) const
    {
        std::remove(log.c_str());
        try {
            op();
        } catch (const std::runtime_error &e) {
            if (!calls().empty())
                return "bectl chamado: " + calls().front();
            return e.what();
        }
        return "sem exceção";
    }
};

bool startsWith(const std::string &s, const std::string &prefix)
{
    return s.rfind(prefix, 0) == 0;
}

} // namespace

TEST_CASE("nomes de BE inválidos são recusados antes do bectl", "[validation]")
{
    Fixture f;
    bectl::Manager mgr("", fakeBectl(f.dir, f.log, 0));
    const std::string name = GENERATE(std::string("-F"), std::string("--help"), std::string(),
                                      std::string("meu be"), std::string("pool/be"), std::string("."),
                                      std::string(".."), std::string("be@snap"), std::string("versão"),
                                      std::string(256, 'a'));
    CAPTURE(name);
    const std::string prefix = "Invalid boot environment name";
    CHECK(startsWith(f.rejected([&] { mgr.create(name); }), prefix));
    CHECK(startsWith(f.rejected([&] { mgr.activate(name); }), prefix));
    CHECK(startsWith(f.rejected([&] { mgr.rename("default", name); }), prefix));
    CHECK(startsWith(f.rejected([&] { mgr.rename(name, "default"); }), prefix));
    CHECK(startsWith(f.rejected([&] { mgr.mount(name); }), prefix));
    CHECK(startsWith(f.rejected([&] { mgr.unmount(name); }), prefix));
}

TEST_CASE("nomes de snapshot inválidos são recusados", "[validation]")
{
    Fixture f;
    bectl::Manager mgr("", fakeBectl(f.dir, f.log, 0));
    const std::string name = GENERATE(std::string("default@"), std::string("default@-r"),
                                      std::string("a@b@c"), std::string("-x@snap"));
    CAPTURE(name);
    CHECK(startsWith(f.rejected([&] { mgr.createSnapshot(name); }), "Invalid"));
    CHECK(startsWith(f.rejected([&] { mgr.create("novo", name); }), "Invalid"));
    CHECK(startsWith(f.rejected([&] { mgr.destroy(name); }), "Invalid"));
}

TEST_CASE("ponto de montagem relativo é recusado", "[validation]")
{
    Fixture f;
    bectl::Manager mgr("", fakeBectl(f.dir, f.log, 0));
    CHECK(startsWith(f.rejected([&] { mgr.mount("default", "mnt"); }),
                     "Mount point must be an absolute path"));
}

TEST_CASE("snapshot sem @ é recusado", "[validation]")
{
    Fixture f;
    bectl::Manager mgr("", fakeBectl(f.dir, f.log, 0));
    CHECK(f.rejected([&] { mgr.createSnapshot("default"); })
          == "Snapshot name must contain '@' (e.g. default@before-upgrade)");
}

TEST_CASE("nomes válidos chegam ao bectl com os argumentos certos", "[validation]")
{
    Fixture f;
    bectl::Manager mgr("", fakeBectl(f.dir, f.log, 0));
    mgr.create("13.2-RELEASE_2026.10:a", "default@snap1", true);
    mgr.createSnapshot("default@antes-upgrade");
    mgr.activate("be1", true);
    mgr.rename("be1", "be2");
    mgr.mount("be2", "/mnt/be2");
    mgr.unmount("be2", true);
    mgr.destroy("be2@snap", true, true);
    CHECK(f.calls() == std::vector<std::string>{
                           "create -r -e default@snap1 13.2-RELEASE_2026.10:a",
                           "create default@antes-upgrade",
                           "activate -t be1",
                           "rename be1 be2",
                           "mount be2 /mnt/be2",
                           "umount -f be2",
                           "destroy -F -o be2@snap",
                       });
}

TEST_CASE("status de saída sem mensagem vira texto", "[validation]")
{
    Fixture f;
    bectl::Manager mgr("", fakeBectl(f.dir, f.log, 3));
    CHECK(f.rejected([&] { mgr.activate("be1"); }) == "bectl chamado: activate be1");
    try {
        mgr.activate("be1");
        FAIL("sem exceção");
    } catch (const std::runtime_error &e) {
        CHECK(std::string(e.what()) == "Command failed with exit code 3");
    }
}

TEST_CASE("mensagem de erro do bectl é repassada", "[validation]")
{
    Fixture f;
    bectl::Manager mgr("", fakeBectl(f.dir, f.log, 1, "echo 'boot environment be1 does not exist' >&2\n"));
    try {
        mgr.activate("be1");
        FAIL("sem exceção");
    } catch (const std::runtime_error &e) {
        CHECK(std::string(e.what()) == "boot environment be1 does not exist");
    }
}

TEST_CASE("mensagens seguem o idioma", "[i18n]")
{
    Fixture f;
    bectl::Manager mgr("", fakeBectl(f.dir, f.log, 0));
    i18n::setLocale("pt_BR");
    CHECK(f.rejected([&] { mgr.create("-F"); }) == "Nome de Boot Environment inválido: -F");
    i18n::setLocale("es_ES");
    CHECK(f.rejected([&] { mgr.create("-F"); }) == "Nombre de Boot Environment no válido: -F");
    i18n::setLocale("en_US");
}

TEST_CASE("elevador ausente é informado", "[validation]")
{
    Fixture f;
    bectl::Manager mgr("doas-que-nao-existe", fakeBectl(f.dir, f.log, 0));
    CHECK(startsWith(f.rejected([&] { mgr.activate("be1"); }), "'doas-que-nao-existe' not found"));
}

// Formato de sbin/bectl/bectl_list.c com -H -s: BE sem recuo; dataset e
// snapshots com dois espaços; origem de clone com quatro.
TEST_CASE("lê a lista de snapshots", "[parse]")
{
    const std::string out =
        "default\n"
        "  zroot/ROOT/default\tNR\t/\t1.2G\t2026-10-01 10:00\n"
        "  zroot/ROOT/default@antes-upgrade\t-\t-\t300M\t2026-10-05 09:30\n"
        "  zroot/ROOT/default@2026-10-10-08:00:00-0\t-\t-\t12K\t2026-10-10 08:00\n"
        "\n"
        "be-clone\n"
        "  zroot/ROOT/be-clone\t-\t-\t8K\t2026-10-10 08:00\n"
        "    zroot/ROOT/default@2026-10-10-08:00:00-0\t-\t-\t12K\t2026-10-10 08:00\n";
    const auto snaps = bectl::parseSnapshots(out);
    REQUIRE(snaps.size() == 2);
    CHECK(snaps[0].name == "default@antes-upgrade");
    CHECK(snaps[0].environment() == "default");
    CHECK(snaps[0].space == "300M");
    CHECK(snaps[0].created == "2026-10-05 09:30");
    CHECK(snaps[1].name == "default@2026-10-10-08:00:00-0");
}

TEST_CASE("lê a lista de BEs, com e sem -H", "[parse]")
{
    const auto tabs = bectl::parseList("default\tNR\t/\t1.2G\t2026-10-01 10:00\nbe2\t-\t-\t10M\t2026-10-02 11:00\n");
    REQUIRE(tabs.size() == 2);
    CHECK(tabs[0].name == "default");
    CHECK(tabs[0].isActiveNow());
    CHECK(tabs[0].isActiveReboot());
    CHECK(tabs[0].isMounted());
    CHECK(tabs[1].created == "2026-10-02 11:00");
    const auto spaces = bectl::parseList("default NR / 1.2G 2026-10-01 10:00");
    REQUIRE(spaces.size() == 1);
    CHECK(spaces[0].created == "2026-10-01 10:00");
}

TEST_CASE("converte tamanhos do bectl em bytes", "[parse]")
{
    using P = std::pair<std::string, std::int64_t>;
    const P p = GENERATE(P{"512B", 512}, P{"0", 0}, P{"24K", 24 * 1024},
                         P{"1.69M", static_cast<std::int64_t>(1.69 * 1024 * 1024)},
                         P{"4.19G", static_cast<std::int64_t>(4.19 * 1024 * 1024 * 1024)},
                         P{"2T", std::int64_t(2) * 1024 * 1024 * 1024 * 1024}, P{"-", -1}, P{"", -1},
                         P{"12X", -1});
    CAPTURE(p.first);
    CHECK(bectl::sizeToBytes(p.first) == p.second);
}

TEST_CASE("tamanhos ordenam numericamente", "[parse]")
{
    // Como texto, "1.2G" < "10M" < "5M"; em bytes, a ordem é a inversa.
    CHECK(bectl::sizeToBytes("5M") < bectl::sizeToBytes("10M"));
    CHECK(bectl::sizeToBytes("10M") < bectl::sizeToBytes("1.2G"));
}

TEST_CASE("execução de processos", "[process]")
{
    SECTION("captura stdout, stderr e status")
    {
        const auto r = bectl::runProcess({"/bin/sh", "-c", "echo out; echo err >&2; exit 4"}, 5000);
        CHECK(r.started);
        CHECK(r.out == "out\n");
        CHECK(r.err == "err\n");
        CHECK(r.status == 4);
    }
    SECTION("executável inexistente")
    {
        const auto r = bectl::runProcess({"/nao/existe/bectl"}, 5000);
        CHECK_FALSE(r.started);
        CHECK_FALSE(r.startError.empty());
    }
    SECTION("tempo limite")
    {
        const auto r = bectl::runProcess({"/bin/sh", "-c", "sleep 5"}, 200);
        CHECK(r.timedOut);
    }
    SECTION("término por sinal")
    {
        const auto r = bectl::runProcess({"/bin/sh", "-c", "kill -9 $$"}, 5000);
        CHECK(r.crashed);
    }
}

TEST_CASE("falha ao iniciar o bectl é informada", "[validation]")
{
    i18n::setLocale("en_US");
    bectl::Manager mgr("", "/nao/existe/bectl");
    try {
        mgr.listEnvironments();
        FAIL("sem exceção");
    } catch (const std::runtime_error &e) {
        CHECK(startsWith(e.what(), "Failed to start '/nao/existe/bectl list -H'"));
    }
}

TEST_CASE("preferências gravam e releem", "[settings]")
{
    TempDir dir;
    const std::string path = dir.file("sub/dir/config");
    {
        bectl::Settings s(path);
        CHECK(s.value("x", "padrão") == "padrão");
        s.setValue("sort.column", 4);
        s.setValue("icons", "ascii");
        REQUIRE(s.save());
    }
    bectl::Settings s(path);
    CHECK(s.intValue("sort.column", -1) == 4);
    CHECK(s.value("icons") == "ascii");
    CHECK(s.intValue("icons", 7) == 7);
}

TEST_CASE("trf substitui os argumentos uma vez", "[i18n]")
{
    i18n::setLocale("en_US");
    CHECK(i18n::trf("Failed to start '%1': %2", {"%2", "x"}) == "Failed to start '%2': x");
    i18n::setLocale("pt_BR");
    CHECK(i18n::trf("List updated (%1 BE(s)).", {"3"}) == "Lista atualizada (3 BE(s)).");
    i18n::setLocale("en_US");
}
