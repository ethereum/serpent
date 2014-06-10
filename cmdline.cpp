#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "compiler.h"
#include "rewriter.h"
#include "parser.h"
#include "lllparser.h"
#include "tokenize.h"
#include "bignum.h"

int main(int argv, char** argc) {
    if (argv == 1) {
        std::cerr << "Must provide a command and arguments! Try parse, rewrite, compile, assemble\n";
    }
    std::string flag = "";
    std::string command = argc[1];
    std::string input;
    std::string inputFile = "main";
    std::string secondInput;
    if (std::string(argc[1]) == "-s") {
        flag = command.substr(1);
        command = argc[2];
        input = "";
        std::string line;
        while (std::getline(std::cin, line)) {
            input += line + "\n";
        }
        secondInput = argv == 3 ? "" : argc[3];
    }
    else {
        if (argv == 2) std::cerr << "Not enough arguments for serpent cmdline\n";
        input = argc[2];
        secondInput = argv == 3 ? "" : argc[3];
    }
    bool haveSec = secondInput.length() > 0;
    if (exists(input)) {
        inputFile = input;
        input = get_file_contents(input);
    }
    if (command == "parse" || command == "parse_serpent") {
        std::cout << printAST(parseSerpent(input, inputFile), haveSec) << "\n";
    }
    else if (command == "rewrite") {
        std::cout << printAST(rewrite(parseLLL(input)), haveSec) << "\n";
    }
    else if (command == "compile_to_lll") {
        std::cout << printAST(rewrite(parseSerpent((input))), haveSec) << "\n";
    }
    else if (command == "compile_lll") {
        std::cout << printAST(compile_lll(parseLLL(input)), haveSec) << "\n";
    }
    else if (command == "dereference") {
        std::cout << printAST(dereference(parseLLL(input)), haveSec) <<"\n";
    }
    else if (command == "pretty_assemble") {
        std::cout << printTokens(flatten(dereference(parseLLL(input)))) <<"\n";
    }
    else if (command == "assemble") {
        std::cout << assemble(parseLLL(input)) << "\n";
    }
    else if (command == "compile") {
        std::cout << assemble(compile_lll(rewrite(
                        parseSerpent(input, inputFile)))) << "\n";
    }

    else if (command == "tokenize") {
        std::cout << printTokens(tokenize(input));
    }
    else if (command == "biject") {
        if (argv == 3)
             std::cerr << "Not enough arguments for biject\n";
        int pos = decimalToInt(secondInput);
        std::vector<Node> n = flatten(dereference(compile_lll(rewrite(
                       parseSerpent(input, inputFile)))));
        if (pos >= (int)n.size())
             std::cerr << "Code position too high\n";
        Metadata m = n[pos].metadata;
        std::cout << "Opcode: " << n[pos].val << ", file: " << m.file << 
             ", line: " << m.ln << ", char: " << m.ch << "\n";
    }
}
