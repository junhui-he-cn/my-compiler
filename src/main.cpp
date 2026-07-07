#include "BytecodeCompiler.hpp"
#include "BytecodeTextEmitter.hpp"
#include "IRCompiler.hpp"
#include "IRInterpreter.hpp"
#include "Lexer.hpp"
#include "ModuleProgram.hpp"
#include "Parser.hpp"
#include "SourceManager.hpp"
#include "TypeChecker.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void printUsage(const char* executable)
{
    std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--bytecode] [--run] [file ...]\n"
              << "       " << executable << " --emit-bytecode output.cdbc file [...]\n"
              << "If no file is provided, source is read from stdin except for --emit-bytecode, which requires at least one file.\n";
}

bool containsModuleToken(const SourceLoadResult& loadResult)
{
    for (const SourceUnit& unit : loadResult.units) {
        Lexer lexer(unit.source);
        for (const Token& token : lexer.scanTokens()) {
            if (token.type == TokenType::Import || token.type == TokenType::Export) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    bool showTokens = false;
    bool showIr = false;
    bool showBytecode = false;
    bool runIr = false;
    std::optional<std::string> emitBytecodePath;
    std::vector<std::string> inputPaths;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--tokens") {
            showTokens = true;
        } else if (arg == "--ir") {
            showIr = true;
        } else if (arg == "--bytecode") {
            showBytecode = true;
        } else if (arg == "--run") {
            runIr = true;
        } else if (arg == "--emit-bytecode") {
            if (i + 1 >= argc) {
                printUsage(argv[0]);
                return 64;
            }
            emitBytecodePath = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else {
            inputPaths.push_back(arg);
        }
    }

    if (emitBytecodePath) {
        if (inputPaths.empty() || showTokens || showIr || showBytecode || runIr) {
            printUsage(argv[0]);
            return 64;
        }
    }

    std::string source;
    try {
        SourceManager sourceManager;
        Program program;
        std::vector<Token> tokens;

        if (inputPaths.empty()) {
            source = sourceManager.loadStdin(std::cin);
            Lexer lexer(source);
            tokens = lexer.scanTokens();
            if (showTokens) {
                for (const Token& token : tokens) {
                    std::cout << token << '\n';
                }
                std::cout << '\n';
            }
            Parser parser(tokens);
            program = parser.parse();
        } else {
            SourceLoadResult loadResult = sourceManager.loadFileUnits(inputPaths);
            source = loadResult.combinedSource;
            if (showTokens) {
                Lexer lexer(source);
                tokens = lexer.scanTokens();
                for (const Token& token : tokens) {
                    std::cout << token << '\n';
                }
                std::cout << '\n';
            }

            if (containsModuleToken(loadResult)) {
                program = buildModuleProgram(loadResult);
            } else {
                Lexer lexer(source);
                tokens = lexer.scanTokens();
                Parser parser(tokens);
                program = parser.parse();
            }
        }

        TypeChecker typeChecker;
        const ResolvedNames& resolvedNames = typeChecker.check(program);

        if (!emitBytecodePath && !showIr && !showBytecode && !runIr) {
            program.print(std::cout);
        }

        if (emitBytecodePath || showIr || showBytecode || runIr) {
            IRCompiler compiler;
            IRProgram ir = compiler.compile(program, resolvedNames);

            std::optional<BytecodeProgram> bytecode;
            if (emitBytecodePath || showBytecode) {
                BytecodeCompiler bytecodeCompiler;
                bytecode = bytecodeCompiler.compile(ir);
            }

            if (emitBytecodePath) {
                std::ostringstream artifact;
                writeBytecodeText(artifact, *bytecode);
                std::ofstream output(*emitBytecodePath);
                if (!output) {
                    throw std::runtime_error("failed to open bytecode output file: " + *emitBytecodePath);
                }
                output << artifact.str();
                if (!output) {
                    throw std::runtime_error("failed to write bytecode output file: " + *emitBytecodePath);
                }
                return 0;
            }

            bool emittedSection = false;
            const auto separateSection = [&emittedSection]() {
                if (emittedSection) {
                    std::cout << '\n';
                }
                emittedSection = true;
            };

            if (showIr) {
                separateSection();
                ir.print(std::cout);
            }

            if (showBytecode) {
                separateSection();
                bytecode->print(std::cout);
            }

            if (runIr) {
                separateSection();
                IRInterpreter interpreter(std::cout);
                interpreter.execute(ir);
            }

        }
    } catch (const DiagnosticError& error) {
        std::cerr << formatDiagnosticWithSource(error, source) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
