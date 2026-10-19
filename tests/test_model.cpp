// Modelo da lista: filtro, ordenação e seleção, com o comportamento do
// bectl-gui 1.2.0.

#include "listmodel.h"

#include <catch2/catch_test_macros.hpp>

using model::SortSpec;

namespace {

std::vector<bectl::BootEnvironment> sample()
{
    // Na ordem do bectl.
    return {
        {"default", "NR", "/", "1.2G", "2026-10-01 10:00"},
        {"be-mounted", "-", "/mnt/be", "10M", "2026-10-02 11:00"},
        {"Be-Idle", "-", "-", "5M", "2026-10-03 12:00"},
    };
}

std::vector<std::string> names(const std::vector<bectl::BootEnvironment> &rows)
{
    std::vector<std::string> out;
    for (const auto &r : rows)
        out.push_back(r.name);
    return out;
}

using Names = std::vector<std::string>;

} // namespace

TEST_CASE("sem ordenação, mantém a ordem do bectl", "[model]")
{
    CHECK(names(model::visibleRows(sample(), "", {})) == Names{"default", "be-mounted", "Be-Idle"});
}

TEST_CASE("filtro pelo nome, sem diferenciar maiúsculas", "[model]")
{
    CHECK(names(model::visibleRows(sample(), "idle", {})) == Names{"Be-Idle"});
    CHECK(names(model::visibleRows(sample(), "BE-", {})) == Names{"be-mounted", "Be-Idle"});
    CHECK(model::visibleRows(sample(), "nada", {}).empty());
}

TEST_CASE("espaço ordena em bytes, não como texto", "[model]")
{
    CHECK(names(model::visibleRows(sample(), "", {model::Space, true}))
          == Names{"Be-Idle", "be-mounted", "default"});
    CHECK(names(model::visibleRows(sample(), "", {model::Space, false}))
          == Names{"default", "be-mounted", "Be-Idle"});
}

TEST_CASE("nome, data, estado e montagem", "[model]")
{
    CHECK(names(model::visibleRows(sample(), "", {model::Name, true}))
          == Names{"Be-Idle", "be-mounted", "default"});
    CHECK(names(model::visibleRows(sample(), "", {model::Created, false}))
          == Names{"Be-Idle", "be-mounted", "default"});
    CHECK(model::visibleRows(sample(), "", {model::StateIcon, true}).front().name == "default");
    CHECK(names(model::visibleRows(sample(), "", {model::Mountpoint, true}))
          == Names{"Be-Idle", "default", "be-mounted"});   // "-" < "/" < "/mnt/be"
}

TEST_CASE("ordenação estável: empates mantêm a ordem do bectl", "[model]")
{
    // Os dois inativos empatam no estado.
    CHECK(names(model::visibleRows(sample(), "", {model::StateIcon, true}))
          == Names{"default", "be-mounted", "Be-Idle"});
}

TEST_CASE("clique no cabeçalho: crescente, decrescente, sem ordem", "[model]")
{
    SortSpec s;
    s = model::nextOnHeaderClick(s, model::Space);
    CHECK(s == SortSpec{model::Space, true});
    s = model::nextOnHeaderClick(s, model::Space);
    CHECK(s == SortSpec{model::Space, false});
    s = model::nextOnHeaderClick(s, model::Space);
    CHECK(s == SortSpec{-1, true});
    s = model::nextOnHeaderClick(SortSpec{model::Space, false}, model::Name);
    CHECK(s == SortSpec{model::Name, true});
}

TEST_CASE("seleção acompanha o BE", "[model]")
{
    const auto bySpace = model::visibleRows(sample(), "", {model::Space, true});
    CHECK(model::rowToSelect(bySpace, "be-mounted") == 1);
    CHECK(model::rowToSelect(bySpace, "sumiu") == 0);
    CHECK(model::rowToSelect({}, "default") == -1);
}

TEST_CASE("snapshots do BE e ordenação", "[model]")
{
    const std::vector<bectl::Snapshot> all{
        {"default@antes", "300M", "2026-10-04 09:00"},
        {"default@depois", "1M", "2026-10-05 09:00"},
        {"be-mounted@x", "1K", "2026-10-06 09:00"},
    };
    CHECK(model::snapshotsOf(all, "default").size() == 2);
    CHECK(model::snapshotsOf(all, "be-idle").empty());
    CHECK(model::snapshotsOf(all, "").size() == 3);
    const auto bySpace = model::sortedSnapshots(model::snapshotsOf(all, "default"), {1, true});
    CHECK(bySpace.front().name == "default@depois");
}
