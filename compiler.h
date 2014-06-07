#ifndef ETHSERP_COMPILER
#define ETHSERP_COMPILER

#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"

// Compiles LLL to assembly
std::vector<Node> compile_lll(Node node);

// Assembly -> "readable machine code"
std::vector<Node> dereference(std::vector<Node> program);

// Readable machine code -> final machine code
std::string serialize(std::vector<Node> derefed);

// Assembly -> final machine code
std::string assemble(std::vector<Node> program);

#endif
