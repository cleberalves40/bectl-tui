#ifndef BECTL_TUI_PROCESS_H
#define BECTL_TUI_PROCESS_H

#include <string>
#include <vector>

namespace bectl {

struct ProcessResult {
    std::string out;
    std::string err;
    int status = -1;          // código de saída, se o processo terminou normalmente
    bool started = false;     // false: não foi possível executar argv[0]
    std::string startError;   // motivo, quando started == false
    bool timedOut = false;
    bool crashed = false;     // terminou por sinal
};

/**
 * Executa argv sem shell (fork + execvp), capturando stdout e stderr.
 * timeoutMs < 0: sem limite (comandos que podem pedir senha no terminal).
 * O processo filho herda o terminal, então doas/sudo podem pedir a senha
 * em /dev/tty.
 */
ProcessResult runProcess(const std::vector<std::string> &argv, int timeoutMs);

/** Procura um executável no PATH; devolve string vazia se não achar. */
std::string findExecutable(const std::string &name);

} // namespace bectl

#endif
