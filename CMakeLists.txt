cmake_minimum_required(VERSION 3.16)

project(compiler_demo LANGUAGES CXX)

# Keep the demo portable by requiring only standard C++17 facilities.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so editors and language servers can index the project.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(compiler_demo
    src/Ast.cpp
    src/IR.cpp
    src/IRCompiler.cpp
    src/IRInterpreter.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Value.cpp
    src/main.cpp
)

target_include_directories(compiler_demo PRIVATE include)

enable_testing()
add_test(
    NAME smoke_register_ir
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/smoke_register_ir.sh $<TARGET_FILE:compiler_demo>
)
