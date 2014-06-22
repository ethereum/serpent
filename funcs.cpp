#include <stdio.h>
#include <iostream>
#include <vector>
#include "funcs.h"
#include "bignum.h"
#include "util.h"
#include "parser.h"
#include "lllparser.h"
#include "compiler.h"
#include "rewriter.h"
#include "tokenize.h"

struct inputPair {
    std::string first;
    std::string second;
};

inputPair inputData(std::string input) {
    inputPair o;
    o.first = input;
    o.second = "main";
    if (exists(input)) {
        o.first = get_file_contents(input);
        o.second = input;
    }
    return o;
}

Node compileToLLL(std::string input) {
    inputPair o = inputData(input);
    return rewrite(parseSerpent(o.first, o.second));
}

std::string compile(std::string input) {
    return compileLLL(compileToLLL(input));
}

std::vector<Node> prettyCompile(std::string input) {
    return prettyCompileLLL(compileToLLL(input));
}
