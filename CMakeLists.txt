cmake_minimum_required(VERSION 3.16)

project(compiler_design LANGUAGES CXX)

find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)

# Keep the demo portable by requiring only standard C++17 facilities.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so editors and language servers can index the project.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(COMPILER_DESIGN_ENABLE_WARNINGS "Enable compiler warning flags for project C++ targets" OFF)

function(compiler_design_apply_warnings target_name)
    if(NOT COMPILER_DESIGN_ENABLE_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_executable(compiler_design
    src/Ast.cpp
    src/Bytecode.cpp
    src/BytecodeTextEmitter.cpp
    src/BytecodeCompiler.cpp
    src/Diagnostic.cpp
    src/FrontendSession.cpp
    src/FlowFacts.cpp
    src/IR.cpp
    src/IRCompiler.cpp
    src/Lexer.cpp
    src/ModuleSymbols.cpp
    src/NativeStdlib.cpp
    src/Parser.cpp
    src/TypeChecker.cpp
    src/TypeUtils.cpp
    src/Value.cpp
    src/main.cpp
)

target_include_directories(compiler_design PRIVATE include)
compiler_design_apply_warnings(compiler_design)

enable_testing()
add_test(
    NAME golden_runner_selftest
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/run_golden_tests_selftest.py
)

add_test(
    NAME cmake_config
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/cmake_config_tests.py ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME ci_workflow
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ci_workflow_tests.py ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME cli_multi_source
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/cli_multi_source_tests.py $<TARGET_FILE:compiler_design> ${CMAKE_SOURCE_DIR}/vm-rs
)

add_test(
    NAME golden
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/run_golden_tests.py $<TARGET_FILE:compiler_design>
)

add_test(
    NAME bytecode_artifacts
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/bytecode_artifact_tests.py $<TARGET_FILE:compiler_design> ${CMAKE_SOURCE_DIR}/vm-rs
)

add_test(
    NAME rust_vm
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/run_rust_vm_tests.py $<TARGET_FILE:compiler_design> ${CMAKE_SOURCE_DIR}/vm-rs --goldens
)

add_executable(frontend_session_tests
    tests/frontend_session_tests.cpp
    src/Ast.cpp
    src/Diagnostic.cpp
    src/FrontendSession.cpp
    src/Lexer.cpp
    src/Parser.cpp
)
target_include_directories(frontend_session_tests PRIVATE include)
compiler_design_apply_warnings(frontend_session_tests)
add_test(NAME frontend_session COMMAND frontend_session_tests)

add_executable(flow_facts_tests
    tests/flow_facts_tests.cpp
    src/Ast.cpp
    src/FlowFacts.cpp
    src/TypeUtils.cpp
)
target_include_directories(flow_facts_tests PRIVATE include)
compiler_design_apply_warnings(flow_facts_tests)
add_test(NAME flow_facts COMMAND flow_facts_tests)

add_executable(module_symbols_tests
    tests/module_symbols_tests.cpp
    src/ModuleSymbols.cpp
    src/TypeUtils.cpp
)
target_include_directories(module_symbols_tests PRIVATE include)
compiler_design_apply_warnings(module_symbols_tests)
add_test(NAME module_symbols COMMAND module_symbols_tests)
