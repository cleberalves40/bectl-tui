#include "bectl.h"
#include "i18n.h"
#include "process.h"

#include <cmath>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace bectl {

namespace {

constexpr int kReadTimeoutMs = 60000;

[[noreturn]] void fail(const std::string &msg)
{
    throw std::runtime_error(msg);
}

std::string trim(const std::string &s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return std::string();
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> split(const std::string &s, char sep)
{
    std::vector<std::string> parts;
    size_t start = 0;
    for (;;) {
        const size_t pos = s.find(sep, start);
        parts.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
        if (pos == std::string::npos)
            break;
        start = pos + 1;
    }
    return parts;
}

std::vector<std::string> lines(const std::string &s)
{
    std::vector<std::string> out;
    for (const auto &l : split(s, '\n'))
        if (!l.empty())
            out.push_back(l);
    return out;
}

std::string join(const std::vector<std::string> &v, const std::string &sep)
{
    std::string out;
    for (size_t i = 0; i < v.size(); ++i)
        out += (i ? sep : std::string()) + v[i];
    return out;
}

// Mesmas regras do libbe (be_validate_name/zfs_name_valid): letras, dígitos e
// "_-.:", sem espaços. Além disso, recusa "-" inicial: o nome vai ao bectl(8)
// como argumento, e ele o interpretaria como opção.
bool validComponent(const std::string &s)
{
    static const std::regex re("^[A-Za-z0-9_.:][A-Za-z0-9_.:-]*$");
    return s.size() <= 255 && s != "." && s != ".." && std::regex_match(s, re);
}

void requireBeName(const std::string &name)
{
    if (!validComponent(name))
        fail(i18n::trf("Invalid boot environment name: %1", {name}));
}

// "be" ou "be@snapshot".
void requireBeOrSnapshot(const std::string &name)
{
    const auto parts = split(name, '@');
    if (parts.size() > 2 || !validComponent(parts.front())
        || (parts.size() == 2 && !validComponent(parts.back())))
        fail(i18n::trf(parts.size() == 1 ? "Invalid boot environment name: %1" : "Invalid snapshot name: %1",
                       {name}));
}

} // namespace

std::int64_t sizeToBytes(const std::string &size)
{
    static const std::regex re("^([0-9]+(?:\\.[0-9]+)?)([BKMGTPE]?)$");
    std::smatch m;
    const std::string s = trim(size);
    if (!std::regex_match(s, m, re))
        return -1;
    static const std::string units = "BKMGTPE";
    const int exp = m[2].length() == 0 ? 0 : static_cast<int>(units.find(m[2].str()));
    double bytes = std::stod(m[1].str());
    for (int i = 0; i < exp; ++i)
        bytes *= 1024.0;
    return static_cast<std::int64_t>(bytes);
}

std::vector<BootEnvironment> parseList(const std::string &output)
{
    static const std::regex ws("\\s+");
    std::vector<BootEnvironment> result;
    for (const auto &line : lines(output)) {
        std::vector<std::string> parts = split(line, '\t');
        if (parts.size() < 5) {
            // Sem -H (colunas alinhadas por espaços): a data também tem espaço.
            parts.clear();
            const std::string t = trim(line);
            for (std::sregex_token_iterator it(t.begin(), t.end(), ws, -1), end; it != end; ++it)
                if (!it->str().empty())
                    parts.push_back(*it);
            if (parts.size() < 5)
                continue;
            std::vector<std::string> date(parts.begin() + 4, parts.end());
            result.push_back({parts[0], parts[1], parts[2], parts[3], join(date, " ")});
        } else {
            result.push_back({parts[0], parts[1], parts[2], parts[3], parts[4]});
        }
    }
    return result;
}

// Com -s, "bectl list -H" imprime cada BE numa linha sem recuo e, com recuo de
// dois espaços, o dataset do BE e seus snapshots ("pool/ROOT/be@snap"); a
// origem de um clone aparece com quatro espaços e também contém "@".
std::vector<Snapshot> parseSnapshots(const std::string &output)
{
    std::vector<Snapshot> result;
    for (const auto &line : lines(output)) {
        if (line.rfind("  ", 0) != 0 || line.rfind("   ", 0) == 0)
            continue;
        const auto parts = split(line.substr(2), '\t');
        if (parts.size() < 5 || parts[0].find('@') == std::string::npos)
            continue;
        result.push_back({parts[0].substr(parts[0].rfind('/') + 1), parts[3], parts[4]});
    }
    return result;
}

Manager::Manager(std::string elevator, std::string bectlPath)
    : m_elevator(std::move(elevator))
    , m_bectlPath(std::move(bectlPath))
{
    if (m_bectlPath.empty())
        m_bectlPath = findExecutable("bectl");
    if (m_bectlPath.empty())
        m_bectlPath = "/sbin/bectl";
}

std::string Manager::runOrThrow(const std::vector<std::string> &args, bool privileged)
{
    std::vector<std::string> cmd;
    const bool elevate = privileged && !m_elevator.empty();
    if (elevate) {
        const std::string tool = findExecutable(m_elevator);
        if (tool.empty())
            fail(i18n::trf("'%1' not found. Install it (pkg install %1) or run bectl-tui as root.",
                           {m_elevator}));
        cmd.push_back(tool);
    }
    cmd.push_back(m_bectlPath);
    cmd.insert(cmd.end(), args.begin(), args.end());
    const std::string shown = join(cmd, " ");

    // Comandos elevados não têm limite de tempo: a espera inclui a senha do
    // doas/sudo, e operações como destroy podem ser longas.
    const ProcessResult r = runProcess(cmd, elevate ? -1 : kReadTimeoutMs);
    if (!r.started)
        fail(i18n::trf("Failed to start '%1': %2", {shown, r.startError}));
    if (r.timedOut)
        fail(i18n::trf("Command timed out (%1 ms): %2", {std::to_string(kReadTimeoutMs), shown}));
    if (r.crashed)
        fail(i18n::trf("Process crashed: %1", {shown}));
    if (r.status != 0) {
        std::string msg = trim(r.err).empty() ? trim(r.out) : trim(r.err);
        if (msg.empty())
            msg = i18n::trf("Command failed with exit code %1", {std::to_string(r.status)});
        fail(msg);
    }
    return trim(r.out);
}

std::vector<BootEnvironment> Manager::listEnvironments()
{
    return parseList(runOrThrow({"list", "-H"}, false));
}

std::optional<BootEnvironment> Manager::getEnvironment(const std::string &name)
{
    for (const auto &be : listEnvironments())
        if (be.name == name)
            return be;
    return std::nullopt;
}

std::vector<Snapshot> Manager::listSnapshots()
{
    return parseSnapshots(runOrThrow({"list", "-H", "-s"}, false));
}

std::string Manager::create(const std::string &name, const std::string &source, bool recursive)
{
    requireBeName(name);
    if (!source.empty())
        requireBeOrSnapshot(source);
    std::vector<std::string> args{"create"};
    if (recursive)
        args.push_back("-r");
    if (!source.empty()) {
        args.push_back("-e");
        args.push_back(source);
    }
    args.push_back(name);
    return runOrThrow(args, true);
}

std::string Manager::createSnapshot(const std::string &name, bool recursive)
{
    if (name.find('@') == std::string::npos)
        fail(i18n::tr("Snapshot name must contain '@' (e.g. default@before-upgrade)"));
    requireBeOrSnapshot(name);
    std::vector<std::string> args{"create"};
    if (recursive)
        args.push_back("-r");
    args.push_back(name);
    return runOrThrow(args, true);
}

std::string Manager::activate(const std::string &name, bool temporary)
{
    requireBeName(name);
    std::vector<std::string> args{"activate"};
    if (temporary)
        args.push_back("-t");
    args.push_back(name);
    return runOrThrow(args, true);
}

std::string Manager::destroy(const std::string &name, bool force, bool origin)
{
    requireBeOrSnapshot(name);
    std::vector<std::string> args{"destroy"};
    if (force)
        args.push_back("-F");
    if (origin)
        args.push_back("-o");
    args.push_back(name);
    return runOrThrow(args, true);
}

DestroyManyResult Manager::destroyMany(const std::vector<std::string> &names, bool force, bool origin,
                                       bool skipActive)
{
    DestroyManyResult result;
    std::string activeName;
    if (skipActive) {
        try {
            for (const auto &be : listEnvironments())
                if (be.isActiveNow()) {
                    activeName = be.name;
                    break;
                }
        } catch (...) {
        }
    }
    for (const auto &name : names) {
        if (skipActive && !activeName.empty() && name == activeName) {
            result.errors.push_back(i18n::trf("%1: cannot destroy the currently active BE", {name}));
            continue;
        }
        try {
            destroy(name, force, origin);
            result.ok.push_back(name);
        } catch (const std::exception &e) {
            result.errors.push_back(name + ": " + e.what());
        }
    }
    return result;
}

std::string Manager::rename(const std::string &oldName, const std::string &newName)
{
    requireBeName(oldName);
    requireBeName(newName);
    return runOrThrow({"rename", oldName, newName}, true);
}

std::string Manager::mount(const std::string &name, const std::string &mountpoint)
{
    requireBeName(name);
    if (!mountpoint.empty() && mountpoint[0] != '/')
        fail(i18n::trf("Mount point must be an absolute path: %1", {mountpoint}));
    std::vector<std::string> args{"mount", name};
    if (!mountpoint.empty())
        args.push_back(mountpoint);
    return runOrThrow(args, true);
}

std::string Manager::unmount(const std::string &name, bool force)
{
    requireBeName(name);
    std::vector<std::string> args{"umount"};
    if (force)
        args.push_back("-f");
    args.push_back(name);
    return runOrThrow(args, true);
}

} // namespace bectl
