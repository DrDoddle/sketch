#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "codegen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semanticanalyzer.hpp"

std::string read_source(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error("Could not open source file: " + path);
    }

    std::string source{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

    if (file.bad())
    {
        throw std::runtime_error("Could not read source file: " + path);
    }

    return source;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr
            << "Usage: " << argv[0] << " <file.doodle>\n";

        return 1;
    }

    try
    {
        const std::string source = read_source(argv[1]);

        Lexer lexer(source);
        Parser parser(lexer);

        auto program = parser.parse();

        SemanticAnalyzer analyzer;
        analyzer.analyze(*program);

        CodeGenerator generator;
        generator.generate(*program);
        generator.write_ir("output.ll");

        std::cout << "Compilation successful!\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}