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

int main()
{
    try
    {
        const std::string source =
            read_source("test_source.doodle");

        // Pass source to your scanner.
        Lexer lexer(source);
        Parser parser(lexer);
        try
        {
            auto program = parser.parse();
            std::cout << "Parsing successful!\n";

            SemanticAnalyzer analyzer;
            analyzer.analyze(*program);

            CodeGenerator generator;
            generator.generate(*program);
            generator.write_ir("output.ll");
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
        }
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
