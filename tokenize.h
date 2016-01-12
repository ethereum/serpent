#ifndef ETHSERP_TOKENIZE
#define ETHSERP_TOKENIZE

#include <vector>
#include <string>
#include "util.h"

int chartype(char c);

std::vector<Node> tokenize(std::string inp,
                           Metadata meta=Metadata(),
                           bool lispMode=false);

#endif
