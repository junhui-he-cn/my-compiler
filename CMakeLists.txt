cmake_minimum_required(VERSION 3.16)

project(compiler_demo LANGUAGES CXX)

find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)

# Keep the demo portable by requiring only standard C++17 facilities.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so editors and language servers can index the project.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(compiler_demo
    src/Ast.cpp
    src/Diagnostic.cpp
    src/IR.cpp
    src/IRCompiler.cpp
    src/IRInterpreter.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/TypeChecker.cpp
    src/Value.cpp
    src/main.cpp
)

target_include_directories(compiler_demo PRIVATE include)

enable_testing()
add_test(
    NAME golden_runner_selftest
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/run_golden_tests_selftest.py
)

add_test(
    NAME golden
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/run_golden_tests.py $<TARGET_FILE:compiler_demo>
)
