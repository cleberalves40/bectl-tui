#include "listmodel.h"

#include <algorithm>
#include <cctype>

namespace model {

namespace {

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

int stateRank(const bectl::BootEnvironment &be)
{
    return be.isActiveNow() ? 0 : be.isActiveReboot() ? 1 : 2;
}

// a < b na coluna indicada. Espaço em bytes ("1.2G" < "10M" como texto),
// estado pela posição (em uso, próximo boot, inativo); data "AAAA-MM-DD HH:MM"
// já ordena como texto.
bool lessThan(const bectl::BootEnvironment &a, const bectl::BootEnvironment &b, int column)
{
    switch (column) {
    case StateIcon: return stateRank(a) < stateRank(b);
    case Name:      return lower(a.name) < lower(b.name);
    case Active:    return a.active < b.active;
    case Mountpoint: return a.mountpoint < b.mountpoint;
    case Space:     return bectl::sizeToBytes(a.space) < bectl::sizeToBytes(b.space);
    case Created:   return a.created < b.created;
    default:        return false;
    }
}

} // namespace

SortSpec nextOnHeaderClick(SortSpec current, int column)
{
    if (current.column != column)
        return {column, true};
    if (current.ascending)
        return {column, false};
    return {-1, true};
}

std::vector<bectl::BootEnvironment> visibleRows(const std::vector<bectl::BootEnvironment> &all,
                                                const std::string &filter, SortSpec sort)
{
    const std::string needle = lower(filter);
    std::vector<bectl::BootEnvironment> rows;
    for (const auto &be : all)
        if (needle.empty() || lower(be.name).find(needle) != std::string::npos)
            rows.push_back(be);
    if (sort.column >= 0 && sort.column < ColumnCount) {
        std::stable_sort(rows.begin(), rows.end(), [&](const auto &a, const auto &b) {
            return sort.ascending ? lessThan(a, b, sort.column) : lessThan(b, a, sort.column);
        });
    }
    return rows;
}

std::vector<bectl::Snapshot> sortedSnapshots(std::vector<bectl::Snapshot> snaps, SortSpec sort)
{
    const auto less = [&](const bectl::Snapshot &a, const bectl::Snapshot &b) {
        switch (sort.column) {
        case 0: return lower(a.name) < lower(b.name);
        case 1: return bectl::sizeToBytes(a.space) < bectl::sizeToBytes(b.space);
        case 2: return a.created < b.created;
        default: return false;
        }
    };
    if (sort.column >= 0 && sort.column <= 2)
        std::stable_sort(snaps.begin(), snaps.end(), [&](const auto &a, const auto &b) {
            return sort.ascending ? less(a, b) : less(b, a);
        });
    return snaps;
}

std::vector<bectl::Snapshot> snapshotsOf(const std::vector<bectl::Snapshot> &all, const std::string &be)
{
    std::vector<bectl::Snapshot> out;
    for (const auto &s : all)
        if (be.empty() || s.environment() == be)
            out.push_back(s);
    return out;
}

int rowToSelect(const std::vector<bectl::BootEnvironment> &rows, const std::string &previous)
{
    if (rows.empty())
        return -1;
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].name == previous)
            return static_cast<int>(i);
    return 0;
}

} // namespace model
