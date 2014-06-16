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

Node compileToLLL(std::string input, std::string inputFile) {
    return rewrite(parseSerpent(input, inputFile));
}

std::string compile(std::string input, std::string inputFile) {
    return compileLLL(compileToLLL(input, inputFile));
}

std::vector<Node> prettyCompile(std::string input, std::string inputFile) {
    return prettyCompileLLL(compileToLLL(input, inputFile));
}
