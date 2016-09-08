#ifndef ETHSERP_FUNCTIONS
#define ETHSERP_FUNCTIONS

#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"
#include "lllparser.h"
#include "bignum.h"
#include "optimize.h"
#include "rewriteutils.h"
#include "preprocess.h"


class argPack {
    public:
        argPack(Node a, Node b, Node c) {
            pre = a;
            datastart = b;
            datasz = c;
        }
    Node pre;
    Node datastart;
    Node datasz;
};

// Get argument types from an old-style signature
strvec oldSignatureToTypes(std::string sig);

// Get the list of argument names for a function
std::vector<std::string> getArgNames(std::vector<Node> args);

// Get the list of argument types for a function
std::vector<std::string> getArgTypes(std::vector<Node> args);

// Convert a list of arguments into a <pre, mstart, msize> node
// triple, given the signature of a function
Node packArguments(std::vector<Node> args, strvec argTypeNames,
                   unsigned functionPrefix, Node inner, Metadata m,
                   bool usePrefix=true);

// Create a node for argument unpacking
Node unpackArguments(std::vector<Node> vars, Metadata m);

#endif
