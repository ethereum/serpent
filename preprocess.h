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

class functionMetadata {
    public:
        functionMetadata(int _id=0, std::string _sig="",
                         strvec _argNames=strvec(), std::string _ot="int") {
            id = _id;
            sig = _sig;
            argNames = _argNames;
            outType = _ot;
            ambiguous = false;
        }
        int id;
        std::string sig;
        std::vector<std::string> argNames;
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
        std::map<int, rewriteRuleSet > customMacros;
        std::map<std::string, std::string> types;
        svObj storageVars;
};

#define preprocessResult std::pair<Node, preprocessAux>

// Populate an svObj with the arguments needed to determine
// the storage position of a node
svObj getStorageVars(svObj pre, Node node, std::string prefix="",
                     int index=0);

// Preprocess a function (see cpp for details)
preprocessResult preprocess(Node inp);

// Make a signature for a file
std::string mkExternLine(Node n);

// Make the javascript/solidity import signature for a contract
std::string mkFullExtern(Node n);

// Get the prefix for a function name/sig combo
unsigned int getPrefix(std::string functionName, std::string signature);

// Get the storage data mapping for a file
std::vector<Node> getDataNodes(Node n);

#endif
