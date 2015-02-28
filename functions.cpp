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

std::string getSignature(std::vector<Node> args) {
    std::string o;
    for (unsigned i = 0; i < args.size(); i++) {
        if (args[i].val == ":" && args[i].args[1].val == "str")
            o += "s";
        else if (args[i].val == ":" && args[i].args[1].val == "arr")
            o += "a";
        else if (args[i].val == ":")
            err("Invalid datatype! Remember to change s -> str and a -> arr for latest version", args[0].metadata);
        else
            o += "i";
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

// Convert a list of arguments into a node containing a
// < datastart, datasz > pair

Node packArguments(std::vector<Node> args, std::string sig,
                      unsigned functionPrefix, Metadata m) {
    // Plain old 32 byte arguments
    std::vector<Node> nargs;
    // Variable-sized arguments
    std::vector<Node> vargs;
    // Is a variable an array?
    std::vector<bool> isArray;
    // Fill up above three argument lists
    int argCount = 0;
    for (unsigned i = 0; i < args.size(); i++) {
        Metadata m = args[i].metadata;
        if (args[i].val == "=") {
            // do nothing
        }
        else {
            // Determine the correct argument type
            char argType;
            if (argCount >= (signed)sig.size())
                err("Too many args. Note that the signature of the function "
                    "that you are using may no longer be valid. "
                    "If you want to be sure, run "
                    "`serpent mk_signature <file>` on the contract you are "
                    "including to determine the correct signature.", m);
            argType = sig[argCount];
            // Integer (also usable for short strings)
            if (argType == 'i') {
                nargs.push_back(args[i]);
            }
            // Long string
            else if (argType == 's') {
                vargs.push_back(args[i]);
                isArray.push_back(false);
            }
            // Array
            else if (argType == 'a') {
                vargs.push_back(args[i]);
                isArray.push_back(true);
            }
            else err("Invalid arg type in signature", m);
            argCount++;
        }
    }
    int static_arg_size = 4 + (vargs.size() + nargs.size()) * 32;
    // Start off by saving the size variables and calculating the total
    msn kwargs;
    kwargs["funid"] = tkn(utd(functionPrefix), m);
    std::string pattern =
        "(with _sztot "+utd(static_arg_size)+"                            "
        "    (with _vars (alloc "+utd(vargs.size() * 64)+")               "
        "        (seq                                                     ";
    for (unsigned i = 0; i < vargs.size(); i++) {
        std::string sizeIncrement = 
            isArray[i] ? "(mul 32 (len _x))" : "(len _x)";
        pattern +=
            "(with _x $vl"+utd(i)+" (seq                 "
            "    (mstore (add _vars "+utd(i * 64)+") (mload (sub _x 32))) "
            "    (mstore (add _vars "+utd(i * 64 + 32)+") _x)             "
            "    (set _sztot (add _sztot "+sizeIncrement+" ))))           ";
        kwargs["vl"+utd(i)] = vargs[i];
    }
    // Allocate memory, and set first four data bytes
    pattern +=
            "(with _datastart (add (alloc (add _sztot 64)) 28) (seq       "
            "    (mstore (sub _datastart 28) $funid)                      ";
    // Copy over size variables
    for (unsigned i = 0; i < vargs.size(); i++) {
        std::string posInDatastart = utd(4 + i * 32);
        std::string varArgSize = utd(i * 64);
        pattern +=
            "    (mstore                                                  "
            "          (add _datastart "+posInDatastart+")                "
            "          (mload (add _vars "+varArgSize+")))                ";
    }
    // Store normal arguments
    for (unsigned i = 0; i < nargs.size(); i++) {
        int v = 4 + (i + vargs.size()) * 32;
        pattern +=
            "    (mstore (add _datastart "+utd(v)+") $"+utd(i)+")         ";
        kwargs[utd(i)] = nargs[i];
    }
    // Loop through variable-sized arguments, store them
    pattern += 
            "    (with _pos (add _datastart "+utd(static_arg_size)+") (seq";
    for (unsigned i = 0; i < vargs.size(); i++) {
        std::string vlArgPos = utd(i * 64 + 32);
        std::string vlSizePos = utd(i * 64);
        std::string copySize =
            isArray[i] ? "(mul 32 (mload (add _vars "+vlSizePos+")))"
                       : "(mload (add _vars "+vlSizePos+"))";
        pattern +=
            "        (mcopy _pos                                   "
            "            (mload (add _vars "+vlArgPos+")) "+copySize+")   "
            "        (set _pos (add _pos "+copySize+"))                   ";
    }
    // Return a 2-item array containing the start and size
    pattern += "     (array_lit _datastart _sztot))))))))";
    std::string prefix = "_temp_"+mkUniqueToken();
    // Fill in pattern, return triple
    return subst(parseLLL(pattern), kwargs, prefix, m);
}

// Create a node for argument unpacking
Node unpackArguments(std::vector<Node> vars, Metadata m) {
    std::vector<std::string> varNames;
    std::vector<std::string> longVarNames;
    std::vector<bool> longVarIsArray;
    // Fill in variable and long variable names, as well as which
    // long variables are arrays and which are strings
    for (unsigned i = 0; i < vars.size(); i++) {
        if (vars[i].val == ":") {
            if (vars[i].args.size() != 2)
                err("Malformed def!", m);
            longVarNames.push_back(vars[i].args[0].val);
            std::string tag = vars[i].args[1].val;
            if (tag == "str")
                longVarIsArray.push_back(false);
            else if (tag == "arr")
                longVarIsArray.push_back(true);
            else
                err("Function value can only be string or array. Remember to change s -> str and a -> arr for latest version", m);
        }
        else {
            varNames.push_back(vars[i].val);
        }
    }
    std::vector<Node> sub;
    if (!varNames.size() && !longVarNames.size()) {
        // do nothing if we have no arguments
    }
    else {
        // Copy over short variables
        for (unsigned i = 0; i < varNames.size(); i++) {
            int pos = 4 + i * 32 + longVarNames.size() * 32;
            sub.push_back(asn("untyped", asn("set",
                              token(varNames[i], m),
                              asn("calldataload", tkn(utd(pos), m), m),
                              m)));
        }
        // Copy over long variables
        if (longVarNames.size() > 0) {
            std::vector<Node> sub2;
            std::string startPos =
                utd(varNames.size() * 32 + longVarNames.size() * 32 + 4);
            Node tot = tkn("_tot", m);
            for (unsigned i = 0; i < longVarNames.size(); i++) {
                std::string sizePos = utd(4 + i * 32);
                Node var = tkn(longVarNames[i], m);
                std::string allocType = longVarIsArray[i] ? "array" : "string";
                Node allocSz = longVarIsArray[i]
                    ? asn("mul", tkn("32"), tkn("sz"))
                    : tkn("sz");
                sub2.push_back(
                    asn("with",
                        tkn("sz"),
                        asn("calldataload", tkn(sizePos)),
                        asn("seq",
                            asn("untyped",
                                   asn("set", var, asn(allocType, tkn("sz")))),
                            asn("calldatacopy",
                                   var,
                                   tot,
                                   allocSz),
                            asn("set", tot, asn("add", tot, allocSz)))));
            }
            std::string prefix = "_temp_"+mkUniqueToken();
            sub.push_back(subst(
                astnode("with", tot, tkn(startPos, m), asn("seq", sub2)),
                msn(),
                prefix,
                m));
        }
    }
    return asn("seq", sub, m);
}
