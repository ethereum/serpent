#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"
#include "lllparser.h"
#include "bignum.h"
#include "rewriteutils.h"
#include "optimize.h"
#include "preprocess.h"
#include "functions.h"
#include "opcodes.h"
#include "keccak-tiny-wrapper.h"

// Convert single-letter types to long types
std::string typeMap(char t) {
    return
        t == 'i' ? "int256"
      : t == 's' ? "bytes"
      : t == 'a' ? "int256[]"
      :            "weird";
}

// Get the one-line argument summary for a function based on its signature
std::string getSummary(std::string functionName, std::string signature) {
    std::string o = functionName + "(";
    for (unsigned i = 0; i < signature.size(); i++) {
        if (signature[i] == 's') {
            o += "string";
        } else {
            o += typeMap(signature[i]);
        }
        if (i != signature.size() - 1) o += ",";
    }
    o += ")";
    return o;
}

// Get the prefix bytes for a function/event based on its signature
std::vector<uint8_t> getSigHash(std::string functionName, std::string signature) {
    return sha3(getSummary(functionName, signature));
}

// Grab the first 4 bytes
unsigned int getLeading4Bytes(std::vector<uint8_t> p) {
    return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}

unsigned int getPrefix(std::string functionName, std::string signature) {
    return getLeading4Bytes(getSigHash(functionName, signature));
}

// Convert a function of the form (def (f x y z) (do stuff)) into
// (if (first byte of ABI is correct) (seq (setup x y z) (do stuff)))
Node convFunction(int prefix4, std::vector<Node> args, Node body) {
    std::string prefix = "_temp"+mkUniqueToken()+"_";
    Metadata m = body.metadata;

    // Collect the list of variable names and variable byte counts
    Node unpack = unpackArguments(args, m);
    // Main LLL-based function body
    return astnode("if",
                   astnode("eq",
                           astnode("get", token("__funid", m), m),
                           token(unsignedToDecimal(prefix4), m),
                           m),
                   astnode("seq", unpack, body, m));
}

// Populate an svObj with the arguments needed to determine
// the storage position of a node
svObj getStorageVars(svObj pre, Node node, std::string prefix,
                     int index) {
    Metadata m = node.metadata;
    if (!pre.globalOffset.size()) pre.globalOffset = "0";
    std::vector<Node> h;
    std::vector<std::string> coefficients;
    // Array accesses or atoms
    if (node.val == "access" || node.type == TOKEN) {
        std::string tot = "1";
        h = listfyStorageAccess(node);
        coefficients.push_back("1");
        for (unsigned i = h.size() - 1; i >= 1; i--) {
            // Array sizes must be constant or at least arithmetically
            // evaluable at compile time
            h[i] = calcArithmetic(h[i], false);
            if (!isNumberLike(h[i]))
                err("Array size must be fixed value", m);
            // Create a list of the coefficient associated with each
            // array index
            coefficients.push_back(decimalMul(coefficients.back(), h[i].val));
        }
    }
    // Tuples
    else {
        int startc;
        // Handle the (fun <fun_astnode> args...) case
        if (node.val == "fun") {
            startc = 1;
            h = listfyStorageAccess(node.args[0]);
        }
        // Handle the (<fun_name> args...) case, which
        // the serpent parser produces when the function
        // is a simple name and not a complex astnode
        else {
            startc = 0;
            h = listfyStorageAccess(token(node.val, m));
        }
        svObj sub = pre;
        sub.globalOffset = "0";
        // Evaluate tuple elements recursively
        for (unsigned i = startc; i < node.args.size(); i++) {
            sub = getStorageVars(sub,
                                 node.args[i],
                                 prefix+h[0].val.substr(2)+".",
                                 i-startc);
        }
        coefficients.push_back(sub.globalOffset);
        for (unsigned i = h.size() - 1; i >= 1; i--) {
            // Array sizes must be constant or at least arithmetically
            // evaluable at compile time
            h[i] = calcArithmetic(h[i], false);
            if (!isNumberLike(h[i]))
               err("Array size must be fixed value", m);
            // Create a list of the coefficient associated with each
            // array index
            coefficients.push_back(decimalMul(coefficients.back(), h[i].val));
        }
        pre.offsets = sub.offsets;
        pre.coefficients = sub.coefficients;
        pre.indices = sub.indices;
        pre.nonfinal = sub.nonfinal;
        pre.nonfinal[prefix+h[0].val.substr(2)] = true;
    }
    pre.coefficients[prefix+h[0].val.substr(2)] = coefficients;
    pre.offsets[prefix+h[0].val.substr(2)] = pre.globalOffset;
    pre.indices[prefix+h[0].val.substr(2)] = index;
    if (decimalGt(tt176, coefficients.back()))
        pre.globalOffset = decimalAdd(pre.globalOffset, coefficients.back());
    return pre;
}

// Type inference on output (may fail if inconsistent or too low-level)
std::string inferType(Node node) {
    Metadata m = node.metadata;
    if (node.type == TOKEN)
        return "void";
    std::string cur;
    if (node.val == "return") {
        if (node.args[0].val == ":") {
            if (node.args[0].args[1].type == ASTNODE)
                err("Invalid type: "+printSimple(node.args[0].args[1]), m);
            else if (node.args[0].args[1].val == "arr")
                cur = "arr";
            else if (node.args[0].args[1].val == "str")
                cur = "str";
            else
                err("Invalid type: "+printSimple(node.args[0].args[1]), m);
        }
        else if (node.args.size() == 1) {
            cur = "int";
        }
        else if (node.args[1].val == "=") {
            if (node.args[1].args[0].val == "items")
                cur = "arr";
            else if (node.args[1].args[0].val == "chars")
                cur = "str";
            else
                err("Invalid type: "+printSimple(node.args[1].args[0]), m);
        }
        else
            err("Invalid return command: "+printSimple(node), m);
    }
    else if (node.val == "~return") {
        return "unknown";
    }
    else cur = "void";
    for (unsigned i = 0; i < node.args.size(); i++) {
        std::string newCur = inferType(node.args[i]);
        if (newCur == "unknown" || newCur == "inconsistent")
            return newCur;
        else if (cur == "void")
            cur = newCur;
        else if (newCur != "void" && cur != newCur) {
            warn("Warning: function return type inconsistent!", m);
            return "inconsistent";
        }
    }
    return cur;
}

// Preprocess input containing functions
//
// localExterns is a map of the form, eg,
//
// { x: { foo: 0, bar: 1, baz: 2 }, y: { qux: 0, foo: 1 } ... }
//
// localExternSigs is a map of the form, eg,
//
// { x : { foo: iii, bar: iis, baz: ia }, y: { qux: i, foo: as } ... }
//
// Signifying that x.foo = 0, x.baz = 2, y.foo = 1, etc
// and that x.foo has three integers as arguments, x.bar has two
// integers and a variable-length string, and baz has an integer
// and an array
//
// globalExterns is a one-level map, eg from above
//
// { foo: 1, bar: 1, baz: 2, qux: 0 }
//
// globalExternSigs is a one-level map, eg from above
//
// { foo: as, bar: iis, baz: ia, qux: i}
//
// Note that globalExterns and globalExternSigs may be ambiguous
// Also, a null signature implies an infinite tail of integers
preprocessResult preprocessInit(Node inp) {
    Metadata m = inp.metadata;
    if (inp.val != "seq")
        inp = astnode("seq", inp, m);
    std::vector<Node> empty = std::vector<Node>();
    Node init = astnode("seq", empty, m);
    Node shared = astnode("seq", empty, m);
    std::vector<Node> any;
    std::vector<Node> functions;
    preprocessAux out = preprocessAux();
    std::map<int, std::string> functionPrefixesUsed;
    int storageDataCount = 0;
    for (unsigned i = 0; i < inp.args.size(); i++) {
        Node obj = inp.args[i];
        // Functions
        if (obj.val == "def") {
            if (obj.args.size() == 0)
                err("Empty def", m);
            // Determine name, arguments, return type, body
            std::string funName = obj.args[0].val;
            std::vector<Node> funArgs = obj.args[0].args;
            Node body = obj.args[1];
            std::string funReturnType = inferType(body);
            if (funReturnType == "unknown" || funReturnType == "inconsistent")
                funReturnType = "";
            // Init, shared and any are special functions
            if (funName == "init" || funName == "shared" || funName == "any") {
                if (obj.args[0].args.size())
                    err(funName+" cannot have arguments", m);
            }
            if (funName == "init") init = body;
            else if (funName == "shared") shared = body;
            else if (funName == "any") any.push_back(body);
            // Other functions
            else {
                // Calculate signature
                std::string sig = getSignature(funArgs);
                // Calculate argument name list
                strvec argNames = getArgNames(funArgs);
                // Get function prefix and check collisions
                std::vector<uint8_t> functionPrefix = getSigHash(obj.args[0].val, sig);
                unsigned int prefix4 = getLeading4Bytes(functionPrefix);
                if (functionPrefixesUsed.count(prefix4)) {
                    err("Hash collision between function prefixes: "
                        + obj.args[0].val
                        + ", "
                        + functionPrefixesUsed[prefix4], m);
                }
                if (out.interns.count(obj.args[0].val)) {
                    err("Defining the same function name twice: "
                        +obj.args[0].val, m);
                }
                // Add function
                functions.push_back(convFunction(prefix4, funArgs, body));
                functionMetadata f = 
                    functionMetadata(functionPrefix, sig, argNames, funReturnType);
                out.interns[funName] = f;
                out.interns[funName + "::" + sig] = f;
                functionPrefixesUsed[prefix4] = obj.args[0].val;
            }
        }
        // Events
        else if (obj.val == "event") {
            if (obj.args.size() == 0)
                err("Empty event def", m);
            std::string eventName = obj.args[0].val;
            std::vector<Node> eventArgs = std::vector<Node>();
            std::vector<bool> indexed;
            int indexedCount = 0;
            for (unsigned i = 0; i < obj.args[0].args.size(); i++) {
                Node arg = obj.args[0].args[i];
                if (arg.type == TOKEN) {
                    eventArgs.push_back(asn(":", arg, tkn("int")));
                    indexed.push_back(false);
                }
                else if (arg.args[1].val == "indexed") {
                    if (arg.args[0].type == TOKEN) {
                        eventArgs.push_back(asn(":", arg.args[0], tkn("int")));
                    }
                    else eventArgs.push_back(arg.args[0]);
                    indexed.push_back(true);
                    indexedCount += 1;
                    if (indexedCount > 3)
                        err("Too many indexed variables", m);
                    if (eventArgs.back().args[1].val != "int")
                        err("Cannot index a "+eventArgs.back().args[1].val, m);
                }
                else if (arg.args[0].type == TOKEN) {
                    eventArgs.push_back(arg);
                    indexed.push_back(false);
                }

                else err("Cannot understand event signature", obj.metadata);
            }
            std::string sig = getSignature(eventArgs);
            strvec argNames = getArgNames(eventArgs);
            std::vector<uint8_t> eventPrefix = getSigHash(eventName, sig);
            functionMetadata f =
                functionMetadata(eventPrefix, sig, argNames, "", indexed);
            if (out.events.count(eventName))
                err("Defining the same event name twice", obj.metadata);
            out.events[eventName] = f;
        }
        // Extern declarations
        else if (obj.val == "extern") {
            std::string externName = obj.args[0].val;
            std::vector<Node> externFuns = obj.args[1].args;
            std::string fun, sig, outsig;
            // Process each function in each extern declaration
            for (unsigned i = 0; i < externFuns.size(); i++) {
                std::string fun, sig, o;
                if (externFuns[i].val != ":") {
                    warn("The extern foo: [bar, ...] extern format is "
                         "deprecated. Please regenerate the signature "
                         "for the contract you are including with "
                         " `serpent mk_signature <file>` and reinsert "
                         "it at your convenience", m);
                    fun = externFuns[i].val;
                    sig = "";
                    o = "";
                }
                else if (externFuns[i].args[0].val != ":") {
                    warn("The foo:i extern format is deprecated. It will "
                         "still work for now but better regenerate the "
                         "signature with `serpent mk_signature <file>` and "
                         "paste the new signature in", m);
                    fun = externFuns[i].args[0].val;
                    sig = externFuns[i].args[1].val;
                    o = "";
                }
                else {
                    fun = externFuns[i].args[0].args[0].val;
                    sig = externFuns[i].args[0].args[1].val;
                    if (sig == "_") sig = "";
                    o = externFuns[i].args[1].val;
                }

                std::string outType = (o == "a") ? "arr"
                                    : (o == "s") ? "str"
                                    : (o == "i") ? "int"
                                    :              "";
                std::vector<uint8_t> functionPrefix = getSigHash(fun, sig);
                functionMetadata f 
                    = functionMetadata(functionPrefix, sig, strvec(), outType);
                if (out.externs.count(fun))
                    out.externs[fun].ambiguous = true;
                else
                    out.externs[fun] = f;
                out.externs[fun + "::" + sig] = f;
            }
        }
        // Custom macros
        else if (obj.val == "macro" || (obj.val == "fun" && obj.args[0].val == "macro")) {
            // Rules for valid macros:
            //
            // There are only four categories of valid macros:
            //
            // 1. a macro where the outer function is something
            // which is NOT an existing valid function/extern/datum
            // 2. a macro of the form set(c(x), d) where c must NOT
            // be an existing valid function/extern/datum
            // 3. something of the form access(c(x)), where c must NOT
            // be an existing valid function/extern/datum
            // 4. something of the form set(access(c(x)), d) where c must
            // NOT be an existing valid function/extern/datum
            // 5. something of the form with(c(x), d, e) where c must
            // NOT be an existing valid function/extern/datum
            bool valid = false;
            Node pattern;
            Node substitution;
            int priority;
            // Priority not set: default zero
            if (obj.val == "macro") {
                pattern = obj.args[0];
                substitution = obj.args[1];
                priority = 0;
            }
            // Specified priority
            else {
                pattern = obj.args[1];
                substitution = obj.args[2];
                if (obj.args[0].args.size())
                    priority = dtu(obj.args[0].args[0].val);
                else
                    priority = 0;
            }
            if (opcode(pattern.val) < 0 && !isValidFunctionName(pattern.val))
                valid = true;
            if (pattern.val == "set" &&
                    opcode(pattern.args[0].val) < 0 &&
                    !isValidFunctionName(pattern.args[0].val))
                valid = true;
            if (pattern.val == "access" &&
                    opcode(pattern.args[0].val) < 0 &&
                    !isValidFunctionName(pattern.args[0].val))
            if (pattern.val == "set" &&
                    pattern.args[0].val == "access" &&
                    opcode(pattern.args[0].args[0].val) < 0 &&
                    !isValidFunctionName(pattern.args[0].args[0].val))
                valid = true;
            if (pattern.val == "with" &&
                    opcode(pattern.args[0].val) < 0 &&
                    !isValidFunctionName(pattern.args[0].val))
                valid = true;
            if (valid) {
                if (!out.customMacros.count(priority))
                    out.customMacros[priority] = rewriteRuleSet();
                out.customMacros[priority].addRule
                    (rewriteRule(pattern, substitution));
            }
            else warn("Macro does not fit valid template: "+printSimple(pattern), m);
        }
        // Variable types
        else if (obj.val == "type") {
            std::string typeName = obj.args[0].val;
            std::vector<Node> vars = obj.args[1].args;
            for (unsigned i = 0; i < vars.size(); i++)
                out.types[vars[i].val] = typeName;
        }
        // Storage variables/structures
        else if (obj.val == "data") {
            out.storageVars = getStorageVars(out.storageVars,
                                             obj.args[0],
                                             "",
                                             storageDataCount);
            storageDataCount += 1;
        }
        else any.push_back(obj);
    }
    // Set up top-level AST structure
    std::vector<Node> main;
    if (shared.args.size()) main.push_back(shared);
    if (init.args.size()) main.push_back(init);

    std::vector<Node> code;
    if (shared.args.size()) code.push_back(shared);
    for (unsigned i = 0; i < any.size(); i++)
        code.push_back(any[i]);
    for (unsigned i = 0; i < functions.size(); i++)
        code.push_back(functions[i]);
    Node codeNode;
    if (functions.size() > 0) {
        codeNode = astnode("with",
                           token("__funid", m),
                           astnode("div",
                                   astnode("calldataload", token("0", m), m),
                                   astnode("exp", tkn("2", m), tkn("224", m)),
                                   m),
                           astnode("seq", code, m),
                           m);
    }
    else codeNode = astnode("seq", code, m);
    main.push_back(astnode("~return",
                           token("0", m),
                           astnode("lll",
                                   codeNode,
                                   token("0", m),
                                   m),
                           m));


    Node result;
    if (main.size() == 1) result = main[0];
    else result = astnode("seq", main, inp.metadata);
    return preprocessResult(result, out);
}

preprocessResult processTypes (preprocessResult pr) {
    preprocessAux aux = pr.second;
    Node node = pr.first;
    if (node.type == TOKEN && aux.types.count(node.val))
        node = asn(aux.types[node.val], node, node.metadata);
    else if (node.val == "untyped")
        return preprocessResult(node.args[0], aux);
    else if (node.val == "outer")
        return preprocessResult(node, aux);
    else {
        for (unsigned i = 0; i < node.args.size(); i++) {
            node.args[i] =
                processTypes(preprocessResult(node.args[i], aux)).first;
        }
    }
    return preprocessResult(node, aux);
}

preprocessResult preprocess(Node n) {
    preprocessResult o = processTypes(preprocessInit(n));
    return o;
}

// Create the signature from a contract, usable for inclusion 
// in other contracts
std::string mkExternLine(Node n) {
    preprocessResult pr = preprocess(flattenSeq(n));
    std::vector<std::string> outNames;
    std::vector<functionMetadata> outMetadata;
    if (!pr.second.interns.size())
        return "extern " + n.metadata.file + ": []";
    for (std::map<std::string, functionMetadata>::iterator it=
            pr.second.interns.begin();
            it != pr.second.interns.end(); it++) {
        if ((*it).first.find("::") == -1) {
            outNames.push_back((*it).first);
            outMetadata.push_back((*it).second);
        }
    }
    std::string o = "extern " + n.metadata.file + ": [";
    for (unsigned i = 0; i < outNames.size(); i++) {
        o += outNames[i] + ":"
           + outMetadata[i].sig
           + (outMetadata[i].sig.size() ? "" : "_") + ":";
        o += outMetadata[i].outType == "str" ? "s"
           : outMetadata[i].outType == "arr" ? "a"
           : outMetadata[i].outType == "int" ? "i"
           :                                   "_";
        
        o += (i < outNames.size() - 1) ? ", " : "]"; 
    }
    return o;
}

// Create the full signature from a contract, usable for
// inclusion in solidity contracts and javascript objects
std::string mkFullExtern(Node n) {
    preprocessResult pr = preprocess(flattenSeq(n));
    std::vector<std::string> outNames;
    std::vector<functionMetadata> outMetadata;
    if (!pr.second.interns.size() && !pr.second.events.size())
        return "[]";
    for (std::map<std::string, functionMetadata>::iterator it=
            pr.second.interns.begin();
            it != pr.second.interns.end(); it++) {
        if ((*it).first.find("::") == -1) {
            outNames.push_back((*it).first);
            outMetadata.push_back((*it).second);
        }
    }
    std::string o = "[";
    for (unsigned i = 0; i < outNames.size(); i++) {
        std::string summary = getSummary(outNames[i], outMetadata[i].sig);
        o += "{\n    \"name\": \""+summary+"\",\n";
        o += "    \"type\": \"function\",\n";
        o += "    \"inputs\": [";
        for (unsigned j = 0; j < outMetadata[i].sig.size(); j++) {
            o += "{ \"name\": \""+outMetadata[i].argNames[j]+
                 "\", \"type\": \""+typeMap(outMetadata[i].sig[j])+"\" }";
            o += (j < outMetadata[i].sig.size() - 1) ? ", " : ""; 
        }
        o += "],\n    \"outputs\": [";
        std::string t = outMetadata[i].outType;
        if (t != "void") {
            std::string name, type;
            if (t == "str") { name = "out"; type = "bytes"; }
            else if (t == "arr") { name = "out"; type = "int256[]"; }
            else if (t == "int") { name = "out"; type = "int256"; }
            else { name = "unknown_out"; type = "int256[]"; }
            o += "{ \"name\": \""+name+"\", \"type\": \""+type+"\" }";
        }
        o += "]\n},\n";
    }
    for (std::map<std::string, functionMetadata>::iterator it=
            pr.second.events.begin();
            it != pr.second.events.end(); it++) {
        std::string name = (*it).first;
        functionMetadata outMetadata = (*it).second;
        std::string summary = getSummary(name, outMetadata.sig);
        o += "{\n    \"name\": \""+summary+"\",\n";
        o += "    \"type\": \"event\",\n";
        o += "    \"inputs\": [";
        for (unsigned j = 0; j < outMetadata.sig.size(); j++) {
            std::string indexed = outMetadata.indexed[j] ? "true" : "false";
            o += "{ \"name\": \""+outMetadata.argNames[j]+
                 "\", \"type\": \""+typeMap(outMetadata.sig[j])+
                 "\", \"indexed\": "+indexed +" }";
            o += (j < outMetadata.sig.size() - 1) ? ", " : ""; 
        }
        o += "]\n},\n";
    }
    return o.substr(0, o.size() - 2) + "]";
}

std::vector<Node> getDataNodes(Node n) {
    Metadata m = n.metadata;
    if (n.val != "seq")
        n = astnode("seq", n, m);
    std::vector<Node> dataNodes;
    for (unsigned i = 0; i < n.args.size(); i++) {
        if (n.args[i].val == "data")
            dataNodes.push_back(n.args[i]);
    }
    return dataNodes;
}
