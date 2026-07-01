#include "IRCompiler.hpp"
#include "IRInterpreter.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "TypeChecker.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string readAll(std::istream& in)
{
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string readFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("failed to open input file: " + path);
    }
    return readAll(file);
}

void printUsage(const char* executable)
{
    std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--run] [file]\n"
              << "If file is omitted, source is read from stdin.\n";
}

} // namespace

int main(int argc, char** argv)
{
    bool showTokens = false;
    bool showIr = false;
    bool runIr = false;
    std::string inputPath;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--tokens") {
            showTokens = true;
        } else if (arg == "--ir") {
            showIr = true;
        } else if (arg == "--run") {
            runIr = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (inputPath.empty()) {
            inputPath = arg;
        } else {
            printUsage(argv[0]);
            return 64;
        }
    }

    try {
        const std::string source = inputPath.empty() ? readAll(std::cin) : readFile(inputPath);

        // The front-end pipeline is intentionally simple: lex source text,
        // parse tokens into an AST, then print the resulting tree.
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.scanTokens();

        if (showTokens) {
            for (const Token& token : tokens) {
                std::cout << token << '\n';
            }
            std::cout << '\n';
        }

        Parser parser(tokens);
        Program program = parser.parse();

        TypeChecker typeChecker;
        const ResolvedNames& resolvedNames = typeChecker.check(program);

        if (!showIr && !runIr) {
            program.print(std::cout);
        }

        if (showIr || runIr) {
            IRCompiler compiler;
            IRProgram ir = compiler.compile(program, resolvedNames);

            if (showIr) {
                ir.print(std::cout);
                if (runIr) {
                    std::cout << '\n';
                }
            }

            if (runIr) {
                IRInterpreter interpreter(std::cout);
                interpreter.execute(ir);
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
