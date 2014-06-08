#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "compiler.h"
#include "rewriter.h"
#include "parser.h"
#include "lllparser.h"
#include "tokenize.h"

int main(int argv, char** argc) {
    if (argv == 1) {
        std::cerr << "Must provide a command and arguments! Try parse, rewrite, compile, assemble\n";
    }
    std::string flag = "";
    std::string command = argc[1];
    std::string input = argc[2];
    if (std::string(argc[1]) == "-s") {
        flag = command.substr(1);
        command = argc[2];
        input = "";
        std::string line;
        while (std::getline(std::cin, line)) {
            input += line + "\n";
        }
    }
    if (exists(input)) {
        input = get_file_contents(input);
    }
    if (command == "parse" || command == "parse_serpent") {
        std::cout << printAST(parseSerpent(input)) << "\n";
    }
    else if (command == "rewrite") {
        std::cout << printAST(rewrite(parseLLL(input))) << "\n";
    }
    else if (command == "compile_lll") {
        std::cout << printTokens(compile_lll(parseLLL(input))) << "\n";
    }
    else if (command == "assemble") {
        std::cout << assemble(tokenize(input)) << "\n";
    }
    else if (command == "compile") {
        std::cout << assemble(compile_lll(rewrite(parseSerpent(input)))) << "\n";
    }

    else if (command == "tokenize") {
        std::cout << printTokens(tokenize(input));
    }
}
