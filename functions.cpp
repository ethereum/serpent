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
#include "functions.h"

strvec oldSignatureToTypes(std::string sig) {
    strvec o;
    for (unsigned i = 0; i < sig.size(); i++) {
        if (sig[i] == 'i')
            o.push_back("int256");
        else if (sig[i] == 's')
            o.push_back("bytes");
        else if (sig[i] == 'a')
            o.push_back("int256[]");
        else
            err("Bad signature: "+sig, Metadata());
    }
    return o;
}

std::vector<std::string> getArgNames(std::vector<Node> args) {
    std::vector<std::string> o;
    for (unsigned i = 0; i < args.size(); i++) {
        if (args[i].val == ":")
            o.push_back(args[i].args[0].val);
        else
            o.push_back(args[i].val);
    }
    return o;
}

std::vector<std::string> getArgTypes(std::vector<Node> args) {
    std::vector<std::string> o;
    for (unsigned i = 0; i < args.size(); i++) {
        if (args[i].val == ":" && args[i].args[1].val == "str")
            o.push_back("bytes");
        else if (args[i].val == ":" && args[i].args[1].val == "arr")
            o.push_back("int256[]");
        else if (args[i].val == ":" && args[i].args[1].val == "access")
            o.push_back(args[i].args[1].args[0].val + "[]");
        else if (args[i].val == ":" && args[i].args[1].val == "int")
            o.push_back("int256");
        else if (args[i].val == ":" && (args[i].args[1].val == "s" ||
                                        args[i].args[1].val == "a"))
            err("Invalid datatype! Remember to change s -> str and a -> arr for latest version", args[0].metadata);
        else if (args[i].val == ":")
            o.push_back(args[i].args[1].val);
        else
            o.push_back("int256");
    }
    return o;
}

// Convert a list of arguments into a node wrapping another
// node that can use _datastart and _datasz

Node packArguments(std::vector<Node> args, strvec argTypeNames,
                   unsigned functionPrefix, Node inner, Metadata m,
                   bool usePrefix) {
    // Arguments
    std::vector<Node> funArgs;
    // Is a variable an array?
    std::vector<int> argTypes;
    // Fill up above three argument lists
    int argCount = 0;
    bool haveVarg = false;
    for (unsigned i = 0; i < args.size(); i++) {
        Metadata m = args[i].metadata;
        if (args[i].val == "=") {
            // do nothing
        }
        else {
            // Determine the correct argument type
            std::string argType;
            if (argCount >= (signed)argTypeNames.size())
                err("Too many args. Note that the signature of the function "
                    "that you are using may no longer be valid. "
                    "If you want to be sure, run "
                    "`serpent mk_signature <file>` on the contract you are "
                    "including to determine the correct signature.", m);
            argType = argTypeNames[argCount];
            funArgs.push_back(args[i]);
            // Array
            if (isArrayType(argType)) {
                argTypes.push_back(ARRAY);
                haveVarg = true;
            }
            // Long string
            else if (argType == "bytes" || argType == "string") {
                argTypes.push_back(BYTES);
                haveVarg = true;
            }
            // Integer (also usable for short strings)
            else if (argType == "int256" or argType == "bytes20" or argType == "address")
                argTypes.push_back(STATIC);
            else {
                argTypes.push_back(STATIC);
            }
            argCount++;
        }
    }
    int static_arg_size = (4 * usePrefix) + (funArgs.size()) * 32;
    int prefixOffset = 4 * usePrefix;
    Node pattern;
    // If we don't have any variable arguments, then we can take a fast track:
    // simply encode everything sequentially as
    // <4 prefix bytes> <arg 1> <arg 2> ...
    if (!haveVarg) {
        // For convenience, we create an allocated memory slice that is
        // slightly too long, and then only use the part from position 28.
        // This lets us do (mstore <start of allocated memory> <prefix>)
        // to store the prefix bytes
        pattern = asn("with",
                           tkn("_datastart"),
                           asn("add",
                               asn("alloc", tkn(utd(static_arg_size + 32))),
                               tkn("28")),
                           asn("seq"));
        if (usePrefix)
            pattern.args[2].args.push_back(
                asn("mstore",
                    asn("sub", tkn("_datastart"), tkn("28")),
                    tkn(utd(functionPrefix))));
        for (unsigned i = 0; i < funArgs.size(); i++) {
            pattern.args[2].args.push_back(
                asn("mstore",
                    asn("add", tkn("_datastart", m), tkn(utd(i * 32 + prefixOffset), m)),
                    funArgs[i],
                    m));
        }
        pattern.args[2].args.push_back(
            asn("with",
                tkn("_datasz", m),
                tkn(utd(static_arg_size), m),
                inner,
                m));
        return pattern;
    }
    // More general case, where we do have some variable arguments
    // Start by allocating memory for the "head", the slice of data
    // that contains only the static arguments. We also allocate some
    // extra memory in the head in order to store the start positions
    // and sizes of all dynamic variables
    pattern = asn("with",
                  tkn("_offset", m),
                  tkn(utd(static_arg_size - prefixOffset), m),
                  asn("with",
                      tkn("_head", m),
                      asn("add",
                          asn("alloc", tkn(utd(static_arg_size * 3 + 32), m), m),
                          tkn("28")),
                      asn("seq")));
    // Add prefix bytes
    if (usePrefix)
        pattern.args[2].args[2].args.push_back(
            asn("mstore",
                asn("sub", tkn("_head"), tkn("28")),
                tkn(utd(functionPrefix))));
    // Add head bytes per argument
    for (unsigned i = 0; i < funArgs.size(); i++) {
        Node headNode;
        // Static-sized variables
        if (argTypes[i] == STATIC) {
            pattern.args[2].args[2].args.push_back(
                asn("mstore",
                    asn("add", tkn("_head", m), tkn(utd(i * 32 + prefixOffset), m)),
                    funArgs[i],
                    m));
        }
        // Dynamic-sized
        else {
            // Compute the total number of bytes needed to represent a variable,
            // including the length prefix
            Node szNode = asn("mload", asn("sub", tkn("_n"), tkn("32")));
            Node varSizeNode;
            if (argTypes[i] == BYTES)
                varSizeNode = asn("add", tkn("32"), asn("ceil32", szNode));
            else
                varSizeNode = asn("add", tkn("32"), asn("mul", tkn("32"), szNode));
 
            pattern.args[2].args[2].args.push_back(
                asn("with",
                    tkn("_n"),
                    funArgs[i],
                    // Compute variable size
                    asn("with",
                        tkn("_sz"),
                        varSizeNode,
                        asn("seq",
                            // Store offset in head
                            asn("mstore",
                                asn("add", tkn("_head"), tkn(utd(i * 32 + prefixOffset))),
                                tkn("_offset")),
                            // Store the _real_ memory start location variable value (ie.
                            // the start of the 32 byte length prefix)
                            asn("mstore",
                                asn("add",
                                    tkn("_head"),
                                    tkn(utd(static_arg_size + i * 32))),
                                asn("sub", tkn("_n"), tkn("32"))),
                            // Store variable size
                            asn("mstore",
                                asn("add",
                                    tkn("_head"),
                                    tkn(utd(static_arg_size * 2 + i * 32))),
                                tkn("_sz")),
                            // Update offset
                            asn("set",
                                tkn("_offset"),
                                asn("add", tkn("_offset"), tkn("_sz")))))));
        }
    }
    // Now that we know the required size of the data array, we can create it
    Node subpattern = asn("with",
                          tkn("_datastart"),
                          asn("alloc", asn("add", tkn("4"), tkn("_offset"))),
                          asn("seq"));
    // And copy the head into it
    subpattern.args[2].args.push_back(
        asn("mcopy",
            tkn("_datastart"),
            tkn("_head"),
            tkn(utd(static_arg_size))));
    // We use the "offset" variable here as a counter of where we are in the
    // data array
    subpattern.args[2].args.push_back(
        asn("set",
            tkn("_offset"),
            asn("add", tkn("_datastart"), tkn(utd(static_arg_size)))));
    // Store the variable-szed arguments
    for (unsigned i = 0; i < funArgs.size(); i++) {
        Node headNode;
        if (argTypes[i] != STATIC) {
            subpattern.args[2].args.push_back(
                asn("with",
                    tkn("_sz"),
                    // Grab the size that we stored in the head
                    asn("mload",
                        asn("add",
                            tkn("_head"),
                            tkn(utd(static_arg_size * 2 + i * 32)))),
                    asn("seq",
                        asn("mcopy",
                            tkn("_offset"),
                            // Grab the start position that we stored
                            // in the head
                            asn("mload",
                                asn("add",
                                    tkn("_head"),
                                    tkn(utd(static_arg_size + i * 32)))),
                            tkn("_sz")),
                        asn("set",
                            tkn("_offset"),
                            asn("add", tkn("_offset"), tkn("_sz"))))));
        }
    }
    subpattern.args[2].args.push_back(
        asn("with",
            tkn("_datasz"),
            asn("sub", tkn("_offset"), tkn("_datastart")),
            inner));
    pattern.args[2].args[2].args.push_back(subpattern);
    return pattern;
}

// Create a node for argument unpacking
Node unpackArguments(std::vector<Node> vars, Metadata m) {
    std::vector<std::string> varNames;
    std::vector<int> varTypes;
    bool haveVarg = false;
    // Fill in variable and long variable names, as well as which
    // long variables are arrays and which are strings
    for (unsigned i = 0; i < vars.size(); i++) {
        if (vars[i].val == ":") {
            if (vars[i].args.size() != 2)
                err("Malformed def!", m);
            varNames.push_back(vars[i].args[0].val);
            std::string tag = vars[i].args[1].val;
            haveVarg = true;
            if (tag == "str" || tag == "bytes" || tag == "string")
                varTypes.push_back(BYTES);
            else if (tag == "access")
                varTypes.push_back(ARRAY);
            else if (isArrayType(tag))
                varTypes.push_back(ARRAY);
            else {
                varTypes.push_back(STATIC);
            }
        }
        else {
            varNames.push_back(vars[i].val);
            varTypes.push_back(STATIC);
        }
    }
    std::vector<Node> sub;
    if (!varNames.size()) {
        // do nothing if we have no arguments
    }
    else if (!haveVarg) {
        for (unsigned i = 0; i < varNames.size(); i++) {
            sub.push_back(
                asn("set",
                    tkn(varNames[i]),
                    asn("calldataload", tkn(utd(4 + 32 * i)))));
        }
    }
    else {
        sub.push_back(asn("with",
                          tkn("_inputdata"),
                          asn("alloc", asn("calldatasize")),
                          asn("seq")));
        sub[0].args[2].args.push_back(asn("calldatacopy",
                                          tkn("_inputdata"),
                                          tkn("4"),
                                          asn("calldatasize")));
        // Copy over short variables
        for (unsigned i = 0; i < varNames.size(); i++) {
            if (varTypes[i] == STATIC) {
                sub[0].args[2].args.push_back(
                    asn("set",
                        tkn(varNames[i]),
                        asn("calldataload", tkn(utd(4 + 32 * i)))));
            }
            else {
                sub[0].args[2].args.push_back(
                    asn("set",
                        tkn(varNames[i]),
                        asn("add", 
                            asn("add",
                                tkn("_inputdata"),
                                tkn("32")),
                            asn("calldataload", tkn(utd(4 + 32 * i))))));
            }
        }
    }
    return asn("seq", sub, m);
}
