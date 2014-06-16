#ifndef ETHSERP_COMPILER
#define ETHSERP_COMPILER

#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"

// Compiled fragtree -> compiled fragtree without labels
Node dereference(Node program);

// Dereferenced fragtree -> opcodes
std::vector<Node> flatten(Node derefed);

// opcodes -> hex
std::string serialize(std::vector<Node> codons);

// Fragtree -> hex
std::string assemble(Node fragTree);

// Fragtree -> opcodes
std::vector<Node> prettyAssemble(Node fragTree);

// LLL -> hex
std::string compileLLL(Node program);

// LLL -> opcodes
std::vector<Node> prettyCompileLLL(Node program);

// hex -> opcodes
std::vector<Node> deserialize(std::string ser);

// Converts a list of integer values to binary transaction data
std::string encodeDatalist(std::vector<std::string> vals);

#endif
