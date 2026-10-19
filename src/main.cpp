#include "app.h"

#include <cstring>
#include <iostream>
#include <string>

namespace {

void usage()
{
    std::cout << "uso: bectl-tui [--icons=nerd|unicode|ascii] [--lang=en_US|pt_BR|es_ES]\n"
                 "                [--config=ARQUIVO] [--version]\n"
                 "\n"
                 "Interface de terminal para o bectl(8). Os ícones \"nerd\" (padrão) exigem\n"
                 "uma fonte Nerd no terminal, como a JetBrainsMono Nerd Font Mono\n"
                 "(pkg install nerd-fonts-jetbrainsmono).\n";
}

bool option(const char *arg, const char *name, std::string *value)
{
    const size_t n = std::strlen(name);
    if (std::strncmp(arg, name, n) != 0 || arg[n] != '=')
        return false;
    *value = arg + n + 1;
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    tui::Options options;
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) {
            usage();
            return 0;
        }
        if (!std::strcmp(a, "--version") || !std::strcmp(a, "-V")) {
            std::cout << "bectl-tui " << BECTL_TUI_VERSION << '\n';
            return 0;
        }
        if (option(a, "--icons", &options.iconMode) || option(a, "--lang", &options.language)
            || option(a, "--config", &options.settingsPath) || option(a, "--bectl", &options.bectlPath))
            continue;
        std::cerr << "bectl-tui: opção desconhecida: " << a << "\n\n";
        usage();
        return 2;
    }
    tui::App app(options);
    return app.run();
}
