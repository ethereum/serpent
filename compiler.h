#ifndef ETHSERP_COMPILER
#define ETHSERP_COMPILER

#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"

// Compiles LLL to assembly
Node compile_lll(Node node);

// Assembly -> "readable machine code"
Node dereference(Node program);

// Readable machine code -> final machine code
std::string serialize(std::vector<Node> codons);

// Assembly -> final machine code
std::string assemble(Node program);

// Assembly -> final machine code
std::vector<Node> flatten(Node derefed);

#endif
