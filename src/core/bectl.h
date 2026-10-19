#ifndef BECTL_TUI_BECTL_H
#define BECTL_TUI_BECTL_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * Integração com o bectl(8): porte em C++ padrão do BectlManager do
 * bectl-gui 1.2.0, com as mesmas regras de validação e a mesma leitura da
 * saída, sem depender do Qt.
 */
namespace bectl {

struct BootEnvironment {
    std::string name;
    std::string active;      // N, R, T, NR, ...
    std::string mountpoint;
    std::string space;
    std::string created;

    bool isActiveNow() const { return active.find('N') != std::string::npos; }
    bool isActiveReboot() const { return active.find('R') != std::string::npos; }
    bool isTemporary() const { return active.find('T') != std::string::npos; }
    bool isMounted() const { return !mountpoint.empty() && mountpoint != "-"; }
};

struct Snapshot {
    std::string name;        // be@snapshot, como os comandos do bectl o aceitam
    std::string space;
    std::string created;

    std::string environment() const { return name.substr(0, name.find('@')); }
};

struct DestroyManyResult {
    std::vector<std::string> ok;
    std::vector<std::string> errors;
};

/**
 * Converte o espaço como o bectl o imprime (humanize_number: "24K", "1.69M",
 * "4.19G") em bytes. Devolve -1 para "-" ou valores não reconhecidos.
 */
std::int64_t sizeToBytes(const std::string &size);

/** Lê a saída de "bectl list -H". */
std::vector<BootEnvironment> parseList(const std::string &output);
/** Lê a saída de "bectl list -H -s" (só os snapshots). */
std::vector<Snapshot> parseSnapshots(const std::string &output);

class Manager
{
public:
    // elevator: comando que eleva privilégios nas escritas ("doas", "sudo"),
    // ou vazio para executar o bectl diretamente (por exemplo, como root).
    // bectlPath: executável no lugar do bectl(8) do PATH; os testes apontam
    // para um bectl falso ou para um wrapper "bectl -r <raiz>".
    explicit Manager(std::string elevator = std::string(), std::string bectlPath = std::string());

    // --- Leitura ---
    std::vector<BootEnvironment> listEnvironments();
    std::optional<BootEnvironment> getEnvironment(const std::string &name);
    std::vector<Snapshot> listSnapshots();

    // --- Escrita (com elevação) ---
    std::string create(const std::string &name, const std::string &source = std::string(),
                       bool recursive = false);
    std::string createSnapshot(const std::string &name, bool recursive = false);
    std::string activate(const std::string &name, bool temporary = false);
    std::string destroy(const std::string &name, bool force = false, bool origin = false);
    DestroyManyResult destroyMany(const std::vector<std::string> &names, bool force = true,
                                  bool origin = false, bool skipActive = true);
    std::string rename(const std::string &oldName, const std::string &newName);
    std::string mount(const std::string &name, const std::string &mountpoint = std::string());
    std::string unmount(const std::string &name, bool force = false);

    const std::string &bectlPath() const { return m_bectlPath; }

private:
    std::string runOrThrow(const std::vector<std::string> &args, bool privileged);

    std::string m_elevator;
    std::string m_bectlPath;
};

} // namespace bectl

#endif
