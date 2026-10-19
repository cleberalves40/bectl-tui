cmake_minimum_required(VERSION 3.16)
project(bectl_tui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# FTXUI (FreeBSD: pkg install ftxui). Sem ele no sistema, como no Ubuntu do CI,
# BECTL_TUI_FETCH_FTXUI baixa a 6.1.9, a versão mais antiga suportada (a do
# FreeBSD/aarch64); o FreeBSD/amd64 tem a 7.x.
option(BECTL_TUI_FETCH_FTXUI "Baixa o FTXUI se não estiver instalado" OFF)
find_package(ftxui QUIET)
if(NOT ftxui_FOUND)
    if(NOT BECTL_TUI_FETCH_FTXUI)
        message(FATAL_ERROR "FTXUI não encontrado: pkg install ftxui, ou -DBECTL_TUI_FETCH_FTXUI=ON")
    endif()
    include(FetchContent)
    FetchContent_Declare(ftxui
        URL https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v6.1.9.tar.gz)
    set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ftxui)
endif()

# Núcleo: bectl(8), validação, leitura da saída, i18n, preferências e o
# modelo da lista. C++ padrão, sem dependências.
add_library(bectl_core STATIC
    src/core/process.cpp
    src/core/bectl.cpp
    src/core/i18n.cpp
    src/core/settings.cpp
    src/model/listmodel.cpp
)
target_include_directories(bectl_core PUBLIC src/core src/model)
target_link_libraries(bectl_core PUBLIC Threads::Threads)

# Interface (FTXUI), compartilhada pelo executável e pelos testes de tela.
add_library(bectl_ui STATIC
    src/tui/icons.cpp
    src/tui/view.cpp
    src/tui/app.cpp
)
target_include_directories(bectl_ui PUBLIC src/tui)
target_link_libraries(bectl_ui PUBLIC bectl_core ftxui::component ftxui::dom ftxui::screen)

add_executable(bectl-tui src/main.cpp)
target_link_libraries(bectl-tui PRIVATE bectl_ui)
# Fonte única da versão: project(... VERSION ...)
target_compile_definitions(bectl-tui PRIVATE BECTL_TUI_VERSION="${PROJECT_VERSION}")

foreach(target bectl_core bectl_ui bectl-tui)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
    )
endforeach()

install(TARGETS bectl-tui RUNTIME DESTINATION bin)

# Testes (Catch2 3). test_integration roda contra o bectl(8) real; veja
# tests/freebsd-e2e.sh.
option(BECTL_TUI_BUILD_TESTS "Compila os testes" OFF)
if(BECTL_TUI_BUILD_TESTS)
    find_package(Catch2 3 REQUIRED)
    enable_testing()
    foreach(test test_core test_model test_view test_integration)
        add_executable(${test} tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE bectl_ui Catch2::Catch2WithMain)
        target_compile_options(${test} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
        )
        # A integração é uma sequência (criar, renomear, ativar, destruir), e o
        # Catch2 3 embaralha a ordem por padrão.
        if(test STREQUAL "test_integration")
            add_test(NAME ${test} COMMAND ${test} --order decl)
        else()
            add_test(NAME ${test} COMMAND ${test})
        endif()
    endforeach()
    # Sem bectl, todos os casos da integração são ignorados e o Catch2 sai com 4.
    set_tests_properties(test_integration PROPERTIES SKIP_RETURN_CODE 4)
endif()
