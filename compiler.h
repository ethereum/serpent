#ifndef ETHSERP_COMPILER
#define ETHSERP_COMPILER

#include <vector>
#include <map>
#include "util.h"

// Compiled fragtree -> compiled fragtree without labels
std::vector<Node> dereference(Node program);

// LLL -> fragtree
Node buildFragmentTree(Node program);

// opcodes -> bin
std::string serialize(std::vector<Node> codons);

// Fragtree -> bin
std::string assemble(Node fragTree);

// Fragtree -> opcodes
std::vector<Node> prettyAssemble(Node fragTree);

// LLL -> bin
std::string compileLLL(Node program);

// LLL -> opcodes
std::vector<Node> prettyCompileLLL(Node program);

// bin -> opcodes
std::vector<Node> deserialize(std::string ser);

#endif
