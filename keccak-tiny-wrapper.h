#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include "keccak-tiny.h"

std::vector<uint8_t> sha3(std::string in) {
    uint8_t o[32];
    sha3_256(o, 32, (uint8_t*)in.c_str(), in.size());
    std::vector<uint8_t> outvec = std::vector<uint8_t>(o, o + 32);
    return outvec;
}
