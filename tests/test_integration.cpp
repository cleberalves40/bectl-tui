// Integração contra o bectl(8) real: porte do tst_bectlmanager do bectl-gui
// 1.2.0, com os mesmos casos e na mesma ordem.
//
// Precisa de root e de um pool ZFS de rascunho organizado como raiz de boot
// environments. tests/freebsd-e2e.sh prepara esse pool e define
// BECTL_TEST_CMD com um wrapper que executa "bectl -r <pool>/ROOT", de modo
// que os boot environments do sistema nunca são tocados. Sem a variável, os
// testes são ignorados.

#include "bectl.h"
#include "support.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <stdexcept>

namespace {

bool enabled()
{
    const char *cmd = std::getenv("BECTL_TEST_CMD");
    return cmd && *cmd;
}

bectl::Manager &mgr()
{
    static bectl::Manager m("", std::getenv("BECTL_TEST_CMD") ? std::getenv("BECTL_TEST_CMD") : "");
    return m;
}

bectl::BootEnvironment be(const std::string &name)
{
    const auto found = mgr().getEnvironment(name);
    if (!found)
        throw std::runtime_error("boot environment não encontrado: " + name);
    return *found;
}

#define REQUIRE_BECTL()                                                                                 \
    if (!enabled())                                                                                     \
    SKIP("BECTL_TEST_CMD não definido; execute via tests/freebsd-e2e.sh")

} // namespace

// O pool começa só com "default", marcado como ativo no próximo boot.
TEST_CASE("lê a saída real do bectl", "[integration]")
{
    REQUIRE_BECTL();
    const auto list = mgr().listEnvironments();
    REQUIRE(list.size() == 1);
    const auto &d = list.front();
    CHECK(d.name == "default");
    CHECK(d.isActiveReboot());
    CHECK_FALSE(d.isActiveNow());
    CHECK_FALSE(d.isMounted());
    CHECK_FALSE(d.space.empty());
    CHECK(d.created.find(' ') != std::string::npos);
}

TEST_CASE("cria a partir de outro BE", "[integration]")
{
    REQUIRE_BECTL();
    mgr().create("be-a", "default");
    CHECK(mgr().getEnvironment("be-a"));
}

TEST_CASE("cria snapshot e o clona", "[integration]")
{
    REQUIRE_BECTL();
    mgr().createSnapshot("default@snap1");
    mgr().create("be-b", "default@snap1");
    CHECK(mgr().getEnvironment("be-b"));
}

TEST_CASE("lista snapshots", "[integration]")
{
    REQUIRE_BECTL();
    const auto snaps = mgr().listSnapshots();
    const auto it = std::find_if(snaps.begin(), snaps.end(),
                                 [](const bectl::Snapshot &s) { return s.name == "default@snap1"; });
    REQUIRE(it != snaps.end());
    CHECK(it->environment() == "default");
    CHECK_FALSE(it->space.empty());
    CHECK(it->created.find(' ') != std::string::npos);
}

TEST_CASE("destrói snapshot", "[integration]")
{
    REQUIRE_BECTL();
    mgr().createSnapshot("default@descartavel");
    mgr().destroy("default@descartavel");
    for (const auto &s : mgr().listSnapshots())
        CHECK(s.name != "default@descartavel");
}

TEST_CASE("snapshot sem @ é recusado", "[integration]")
{
    REQUIRE_BECTL();
    CHECK_THROWS_AS(mgr().createSnapshot("default"), std::runtime_error);
}

TEST_CASE("nome duplicado falha", "[integration]")
{
    REQUIRE_BECTL();
    CHECK_THROWS_AS(mgr().create("be-a", "default"), std::runtime_error);
}

TEST_CASE("renomeia", "[integration]")
{
    REQUIRE_BECTL();
    mgr().rename("be-a", "be-renamed");
    CHECK_FALSE(mgr().getEnvironment("be-a"));
    CHECK(mgr().getEnvironment("be-renamed"));
}

TEST_CASE("ativa", "[integration]")
{
    REQUIRE_BECTL();
    mgr().activate("be-renamed");
    CHECK(be("be-renamed").isActiveReboot());
    CHECK_FALSE(be("default").isActiveReboot());
}

TEST_CASE("ativa temporariamente", "[integration]")
{
    REQUIRE_BECTL();
    mgr().activate("be-b", true);
    CHECK(be("be-b").isTemporary());
}

TEST_CASE("monta e desmonta", "[integration]")
{
    REQUIRE_BECTL();
    TempDir dir;
    mgr().mount("be-b", dir.path);
    CHECK(be("be-b").isMounted());
    mgr().unmount("be-b");
    CHECK_FALSE(be("be-b").isMounted());
}

TEST_CASE("destruição em lote informa cada resultado", "[integration]")
{
    REQUIRE_BECTL();
    mgr().activate("default");
    const auto r = mgr().destroyMany({"be-b", "be-renamed", "does-not-exist"});
    CHECK(r.ok == std::vector<std::string>{"be-b", "be-renamed"});
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors.front().rfind("does-not-exist: ", 0) == 0);
    CHECK(mgr().listEnvironments().size() == 1);
}
