#ifndef ETHSERP_PREPROCESSOR
#define ETHSERP_PREPROCESSOR

#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"
#include "rewriteutils.h"

// Storage variable index storing object
struct svObj {
    std::map<std::string, std::string> offsets;
    std::map<std::string, int> indices;
    std::map<std::string, std::vector<std::string> > coefficients;
    std::map<std::string, bool> nonfinal;
    std::string globalOffset;
};

// Grab the first 4 bytes
unsigned int getLeading4Bytes(std::vector<uint8_t> p);

class functionMetadata {
    public:
        functionMetadata(std::vector<uint8_t> _prefix=zeroes(32),
                         strvec _argTypes=strvec(), strvec _argNames=strvec(),
                         std::string _ot="int256",
                         std::vector<bool> _indexed=falses(0)) {
            prefix = _prefix;
            id = getLeading4Bytes(prefix);
            argTypes = _argTypes;
            argNames = _argNames;
            outType = _ot;
            indexed = _indexed;
            if (!indexed.size()) indexed = falses(argNames.size());
            ambiguous = false;
        }
        int id;
        std::vector<uint8_t> prefix;
        std::vector<std::string> argTypes;
        std::vector<std::string> argNames;
        std::vector<bool> indexed;
        std::string outType;
        bool ambiguous;
};


// Preprocessing result storing object
class preprocessAux {
    public:
        preprocessAux() {
        }
        std::map<std::string, functionMetadata> externs;
        std::map<std::string, functionMetadata> interns;
        std::map<std::string, functionMetadata> events;
        std::map<int, rewriteRuleSet > customMacros;
        std::map<std::string, std::string> types;
        svObj storageVars;
};

#define preprocessResult std::pair<Node, preprocessAux>

// Populate an svObj with the arguments needed to determine
// the storage position of a node
svObj getStorageVars(svObj pre, Node node, std::string prefix="",
                     int index=0);

// Is the type a type of an array?
bool isArrayType(std::string type);

// Preprocess a function (see cpp for details)
preprocessResult preprocess(Node inp);

// Make a signature for a file
std::string mkExternLine(Node n);

// Make the javascript/solidity import signature for a contract
std::string mkFullExtern(Node n);

// Get the storage data mapping for a file
std::vector<Node> getDataNodes(Node n);

#endif
