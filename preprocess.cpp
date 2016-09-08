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

// Is the type a type of an array?
bool isArrayType(std::string type) {
    if (type == "arr")
        return true;
    return type.length() >= 2 && type[type.length() - 2] == '['
                              && type[type.length() - 1] == ']';
}

// Get the one-line argument summary for a function based on its signature
std::string getSummary(std::string functionName, strvec argTypes) {
    std::string o = functionName + "(";
    for (unsigned i = 0; i < argTypes.size(); i++) {
        o += argTypes[i];
        if (i != argTypes.size() - 1) o += ",";
    }
    o += ")";
    return o;
}

// Get the prefix bytes for a function/event based on its signature
std::vector<uint8_t> getSigHash(std::string functionName, strvec argTypes) {
    return sha3(getSummary(functionName, argTypes));
}

// Grab the first 4 bytes
unsigned int getLeading4Bytes(std::vector<uint8_t> p) {
    return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}

std::string functionPrefixToHex(unsigned int fprefix) {
    std::string o = "";
    std::string alpha = "01235789abcdef";
    for (unsigned i = 0; i < 8; i++) {
        o = alpha[fprefix % 16] + o;
        fprefix /= 16;
    }
    return o;
}

unsigned int getPrefix(std::string functionName, strvec argTypes) {
    return getLeading4Bytes(getSigHash(functionName, argTypes));
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
            if (node.args[0].args[1].type == ASTNODE) {
                if (node.args[0].args[1].val == "access" &&
                    node.args[0].args[1].args.size() == 1)
                    cur = node.args[0].args[1].args[0].val + "[]";
                else
                    err("Invalid type: "+printSimple(node.args[0].args[1]), m);
            }
            else if (node.args[0].args[1].val == "arr")
                cur = "int256[]";
            else if (node.args[0].args[1].val == "str")
                cur = "bytes";
            else {
                // warn("Non-standard type: "+printSimple(node.args[0].args[1]), m);
                cur = node.args[0].args[1].val;
            }
        }
        else if (node.args.size() == 1) {
            cur = "int256";
        }
        else if (node.args[1].val == "=") {
            if (node.args[1].args[0].val == "items")
                cur = "int256[]";
            else if (node.args[1].args[0].val == "chars")
                cur = "bytes";
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

typeMetadata getTypes(Node typeNode) {
    std::string fun;
    Node sigNode;
    strvec inTypes;
    Node o;
    Metadata m = typeNode.metadata;
    if (typeNode.val != ":") {
        warn("The extern foo: [bar, ...] extern format is "
             "deprecated. Please regenerate the signature "
             "for the contract you are including with "
             " `serpent mk_signature <file>` and reinsert "
             "it at your convenience", m);
        fun = typeNode.val;
        sigNode = tkn("");
        o = tkn("");
    }
    else if (typeNode.args[0].val != ":") {
        warn("The foo:i extern format is deprecated. It will "
             "still work for now but better regenerate the "
             "signature with `serpent mk_signature <file>` and "
             "paste the new signature in", m);
        fun = typeNode.args[0].val;
        sigNode = typeNode.args[1];
        o = tkn("");
    }
    else {
        fun = typeNode.args[0].args[0].val;
        sigNode = typeNode.args[0].args[1];
        o = typeNode.args[1];
    }
    if (sigNode.type == TOKEN)
        inTypes = oldSignatureToTypes(sigNode.val);
    else {
        inTypes = strvec();
        for (unsigned i = 0; i < sigNode.args.size(); i++) {
            if (sigNode.args[i].val == "access" && sigNode.args[i].type == ASTNODE) {
                if (sigNode.args[i].args.size() > 1)
                    err("Fixed-size arrays not supported", sigNode.metadata);
                inTypes.push_back(sigNode.args[i].args[0].val + "[]");
            }
            else inTypes.push_back(sigNode.args[i].val);
        }
    }
    std::string outType;
    if (o.type == ASTNODE && o.val == "access" && o.args.size() == 1)
        outType = o.args[0].val + "[]";
    else if (o.type == TOKEN) {
        outType = (o.val == "a") ? "int256[]"
                : (o.val == "s") ? "bytes"
                : (o.val == "i") ? "int256"
                :              o.val;
    }
    else err ("Invalid out type: "+printSimple(o), m);
    return typeMetadata(fun, inTypes, outType);
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
    std::vector<Node> finally;
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
            // Check if method is constant
            bool isConstant = false;
            if (obj.args[0].val == "const") {
                std::vector<Node> nargs;
                for (unsigned i = 1; i < obj.args.size(); i++)
                    nargs.push_back(obj.args[i]);
                obj.args = nargs;
                isConstant = true;
            }
            // Determine name, arguments, return type, body
            std::string funName = obj.args[0].val;
            std::vector<Node> funArgs = obj.args[0].args;
            Node body = obj.args[1];
            std::string funReturnType = inferType(body);
            if (funReturnType == "unknown" || funReturnType == "inconsistent")
                funReturnType = "";
            if (funReturnType == "void")
                funReturnType = "_";
            // Init, shared and any are special functions
            if (funName == "init" || funName == "shared" || funName == "any" || funName == "finally") {
                if (obj.args[0].args.size())
                    err(funName+" cannot have arguments", m);
            }
            if (funName == "init") init = body;
            else if (funName == "shared") shared = body;
            else if (funName == "any") any.push_back(body);
            else if (funName == "finally") finally.push_back(body);
            // Other functions
            else {
                // Calculate argument name list
                strvec argNames = getArgNames(funArgs);
                // Calculate argument type list
                strvec argTypes = getArgTypes(funArgs);
                // Get function prefix and check collisions
                std::vector<uint8_t> functionPrefix = getSigHash(obj.args[0].val, argTypes);
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
                    functionMetadata(functionPrefix, argTypes, argNames, funReturnType);
                f.constant = isConstant;
                out.interns[funName] = f;
                out.interns[funName + "::" + functionPrefixToHex(getLeading4Bytes(functionPrefix))] = f;
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
                    eventArgs.push_back(asn(":", arg, tkn("int256")));
                    indexed.push_back(false);
                }
                else if (arg.args[1].val == "indexed") {
                    if (arg.args[0].type == TOKEN) {
                        eventArgs.push_back(asn(":", arg.args[0], tkn("int256")));
                    }
                    else eventArgs.push_back(arg.args[0]);
                    indexed.push_back(true);
                    indexedCount += 1;
                    if (eventArgs.back().args[1].val != "int256") {
                        // warn("Non-standard indexed data type", m);
                    }
                    if (isArrayType(eventArgs.back().args[1].val))
                        err("Cannot index an array: " + eventArgs.back().args[1].val, m);
                    if (indexedCount > 3)
                        err("Too many indexed variables", m);
                }
                else if (arg.args[0].type == TOKEN) {
                    eventArgs.push_back(arg);
                    indexed.push_back(false);
                }

                else err("Cannot understand event signature", obj.metadata);
            }
            strvec argNames = getArgNames(eventArgs);
            strvec argTypes = getArgTypes(eventArgs);
            strvec argTypesForPrefixing = strvec();
            for (unsigned i = 0; i < argTypes.size(); i++) {
                if ((argTypes[i] == "bytes" || argTypes[i] == "string") && indexed[i]) {
                    argTypesForPrefixing.push_back(argTypes[i]);
                    argTypesForPrefixing.push_back("bytes32");
                }
                else argTypesForPrefixing.push_back(argTypes[i]);
            }
            std::vector<uint8_t> eventPrefix = getSigHash(eventName, argTypesForPrefixing);
            functionMetadata f =
                functionMetadata(eventPrefix, argTypes, argNames, "", indexed);
            if (out.events.count(eventName))
                err("Defining the same event name twice", obj.metadata);
            out.events[eventName] = f;
        }
        // Extern declarations
        else if (obj.val == "extern") {
            std::string externName = obj.args[0].val;
            std::vector<Node> externFuns = obj.args[1].args;
            // Process each function in each extern declaration
            for (unsigned i = 0; i < externFuns.size(); i++) {
                typeMetadata types = getTypes(externFuns[i]);
                strvec inTypes = types.inTypes;
                std::string outType = types.outType;
                std::string fun = types.name;
                std::vector<uint8_t> functionPrefix = getSigHash(fun, inTypes);
                functionMetadata f 
                    = functionMetadata(functionPrefix, inTypes, strvec(), outType);
                if (out.externs.count(fun) and out.externs[fun].prefix != functionPrefix)
                    out.externs[fun].ambiguous = true;
                else
                    out.externs[fun] = f;
                out.externs[fun + "::" + functionPrefixToHex(getLeading4Bytes(functionPrefix))] = f;
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
            if (obj.args[1].type == TOKEN) {
                out.types["_prefix:"+obj.args[1].val] = obj.args[0].val;
            }
            else {
                std::string typeName = obj.args[0].val;
                std::vector<Node> vars = obj.args[1].args;
                for (unsigned i = 0; i < vars.size(); i++)
                    out.types[vars[i].val] = typeName;
            }
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
    for (unsigned i = 0; i < finally.size(); i++)
        code.push_back(finally[i]);
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
    if (node.type == TOKEN && aux.types.size()) {
        if (aux.types.count(node.val)) {
            node = asn(aux.types[node.val], node, node.metadata);
            return preprocessResult(node, aux);
        }
        for (int i = node.val.size() - 1; i >= 1; i--) {
            std::string prefix = "_prefix:"+node.val.substr(0, i);
            if (aux.types.count(prefix)) {
                node = asn(aux.types[prefix], node, node.metadata);
                return preprocessResult(node, aux);
            }
        }
    }
    if (node.val == "untyped")
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
        o += outNames[i] + ":[";
        for (unsigned j = 0; j < outMetadata[i].argTypes.size(); j++) {
            o += outMetadata[i].argTypes[j];
            if (j < outMetadata[i].argTypes.size() - 1) o += ",";
        }
        o += "]:";
        o += outMetadata[i].outType;
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
        std::string summary = getSummary(outNames[i], outMetadata[i].argTypes);
        std::string constant = outMetadata[i].constant ? "true" : "false";
        o += "{\n    \"name\": \""+summary+"\",\n";
        o += "    \"type\": \"function\",\n";
        o += "    \"constant\": " + constant + ",\n";
        o += "    \"inputs\": [";
        for (unsigned j = 0; j < outMetadata[i].argTypes.size(); j++) {
            o += "{ \"name\": \""+outMetadata[i].argNames[j]+
                 "\", \"type\": \""+outMetadata[i].argTypes[j]+"\" }";
            o += (j < outMetadata[i].argTypes.size() - 1) ? ", " : ""; 
        }
        o += "],\n    \"outputs\": [";
        std::string t = outMetadata[i].outType;
        if (t != "_") {
            std::string name, type;
            if (t != "") { name = "out"; type = t; }
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
        std::string summary = getSummary(name, outMetadata.argTypes);
        o += "{\n    \"name\": \""+summary+"\",\n";
        o += "    \"type\": \"event\",\n";
        o += "    \"inputs\": [";
        for (unsigned j = 0; j < outMetadata.argTypes.size(); j++) {
            // Special handling for indexed bytes/string
            if (outMetadata.indexed[j] && (outMetadata.argTypes[j] == "bytes" ||
                                           outMetadata.argTypes[j] == "string")) {
                o += "{ \"name\": \""+outMetadata.argNames[j]+
                     "\", \"type\": \""+outMetadata.argTypes[j]+
                     "\", \"indexed\": false }, ";
                o += "{ \"name\": \"__hash_"+outMetadata.argNames[j]+
                     "\", \"type\": \"bytes32" +
                     "\", \"indexed\": true }";
                o += (j < outMetadata.argTypes.size() - 1) ? ", " : ""; 
            }
            else {
                std::string indexed = outMetadata.indexed[j] ? "true" : "false";
                o += "{ \"name\": \""+outMetadata.argNames[j]+
                     "\", \"type\": \""+outMetadata.argTypes[j]+
                     "\", \"indexed\": "+indexed +" }";
                o += (j < outMetadata.argTypes.size() - 1) ? ", " : ""; 
            }
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
