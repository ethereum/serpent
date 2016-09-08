#include <stdio.h>
#include <string>
#include <iostream>
#include <vector>
#include <map>
#include "funcs.h"
#include "bignum.h"

static void print_help(const char *program_name) {
       std::cout << "Usage: " << program_name <<
      " command input\n"
      "Where input -s for from stdin, a file, or interpreted as serpent code if does not exist as file."
      "where command: \n"
      " parse:          Just parses and returns s-expression code.\n"
      " rewrite:        Parse, use rewrite rules print s-expressions of result.\n"
      " compile:        Return resulting compiled EVM code in hex.\n"
      " assemble:       Return result from step before compilation.\n";
}

int main(int argv, char** argc) {
    if (argv == 1) {
        std::cerr << "Must provide a command and arguments! Try parse, rewrite, compile, assemble\n";
        return 0;
    }
    std::string command = argc[1];
    if (command == "--help" || command == "-h") {
        print_help(argc[0]);
        return 0;
    }
    std::string input;
    std::string secondInput;
    if (argv == 2) {
         std::cerr << "Not enough arguments for serpent cmdline\n";
         return 1;
    }
    input = argc[2];
    secondInput = argv == 3 ? "" : argc[3];
    bool haveSec = secondInput.length() > 0;
    if (command == "parse" || command == "parse_serpent") {
        std::cout << printAST(parseSerpent(input), haveSec) << "\n";
    }
    else if (command == "rewrite") {
        std::cout << printAST(rewrite(parseLLL(input, true)), haveSec) << "\n";
    }
    else if (command == "compile_to_lll") {
        std::cout << printAST(compileToLLL(input), haveSec) << "\n";
    }
    else if (command == "build_fragtree") {
        std::cout << printAST(buildFragmentTree(parseLLL(input, true))) << "\n";
    }
    else if (command == "compile_lll") {
        std::cout << binToHex(compileLLL(parseLLL(input, true))) << "\n";
    }
    else if (command == "dereference") {
        std::cout << printTokens(dereference(parseLLL(input, true))) <<"\n";
    }
    else if (command == "pretty_assemble") {
        std::cout << printTokens(prettyAssemble(parseLLL(input, true))) <<"\n";
    }
    else if (command == "pretty_compile_lll") {
        std::cout << printTokens(prettyCompileLLL(parseLLL(input, true))) << "\n";
    }
    else if (command == "pretty_compile") {
        std::cout << printTokens(prettyCompile(input)) << "\n";
    }
    else if (command == "assemble") {
        std::cout << assemble(parseLLL(input, true)) << "\n";
    }
    else if (command == "serialize") {
        std::cout << binToHex(serialize(tokenize(input, Metadata(), false))) << "\n";
    }
    else if (command == "deserialize") {
        std::cout << printTokens(deserialize(hexToBin(input))) << "\n";
    }
    else if (command == "compile") {
        std::cout << binToHex(compile(input)) << "\n";
    }
    else if (command == "mk_signature") {
        std::cout << mkSignature(input) << "\n";
    }
    else if (command == "mk_full_signature") {
        std::cout << mkFullSignature(input) << "\n";
    }
    else if (command == "mk_contract_info_decl") {
        std::cout << mkContractInfoDecl(input) << "\n";
    }
    else if (command == "get_prefix") {
        std::cout << getPrefix(input) << "\n";
    }
    else if (command == "tokenize") {
        std::cout << printTokens(tokenize(input));
    }
    else if (command == "biject") {
        if (argv == 3)
             std::cerr << "Not enough arguments for biject\n";
        int pos = decimalToUnsigned(secondInput);
        std::vector<Node> n = prettyCompile(input);
        if (pos >= (int)n.size())
             std::cerr << "Code position too high\n";
        Metadata m = n[pos].metadata;
        std::cout << "Opcode: " << n[pos].val << ", file: " << m.file << 
             ", line: " << m.ln << ", char: " << m.ch << "\n";
    } else {
        std::cerr << "Unknown command" << std::endl;
        print_help(argc[0]);
        return 1;
    }
    return 0;
}
