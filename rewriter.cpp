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

std::string valid[][3] = {
    { "if", "2", "3" },
    { "unless", "2", "2" },
    { "while", "2", "2" },
    { "until", "2", "2" },
    { "alloc", "1", "1" },
    { "array", "1", "1" },
    { "call", "2", tt256 },
    { "call_code", "2", tt256 },
    { "create", "1", "4" },
    { "getch", "2", "2" },
    { "setch", "3", "3" },
    { "sha3", "1", "2" },
    { "return", "1", "2" },
    { "inset", "1", "1" },
    { "min", "2", "2" },
    { "max", "2", "2" },
    { "array_lit", "0", tt256 },
    { "seq", "0", tt256 },
    { "log", "1", "6" },
    { "outer", "1", "1" },
    { "set", "2", "2" },
    { "---END---", "", "" } //Keep this line at the end of the list
};

std::string macros[][2] = {
    {
        "(+= $a $b)",
        "(set $a (+ $a $b))"
    },
    {
        "(*= $a $b)",
        "(set $a (* $a $b))"
    },
    {
        "(-= $a $b)",
        "(set $a (- $a $b))"
    },
    {
        "(/= $a $b)",
        "(set $a (/ $a $b))"
    },
    {
        "(%= $a $b)",
        "(set $a (% $a $b))"
    },
    {
        "(^= $a $b)",
        "(set $a (^ $a $b))"
    },
    {
        "(!= $a $b)",
        "(iszero (eq $a $b))"
    },
    {
        "(assert $x)",
        "(unless $x (stop))"
    },
    {
        "(min $a $b)",
        "(if (lt $a $b) $a $b)"
    },
    {
        "(max $a $b)",
        "(if (lt $a $b) $b $a)"
    },
    {
        "(if @cond $do (else $else))",
        "(if @cond $do $else)"
    },
    {
        "(code $code)",
        "$code"
    },
    {
        "(access (. msg data) $ind)",
        "(calldataload (mul 32 $ind))"
    },
    {
        "(slice $arr $pos)",
        "(add $arr (mul 32 $pos))",
    },
    {
        "(array $len)",
        "(alloc (mul 32 $len))"
    },
    {
        "(while @cond @do)",
        "(until (iszero @cond) @do)",
    },
    {
        "(while (iszero @cond) @do)",
        "(until @cond @do)",
    },
    {
        "(if @cond @do)",
        "(unless (iszero @cond) @do)",
    },
    {
        "(if (iszero @cond) @do)",
        "(unless @cond @do)",
    },
    {
        "(access (. self storage) $ind)",
        "(sload $ind)"
    },
    {
        "(access $var $ind)",
        "(mload (add $var (mul 32 $ind)))"
    },
    {
        "(set (access (. self storage) $ind) $val)",
        "(sstore $ind $val)"
    },
    {
        "(set (access $var $ind) $val)",
        "(mstore (add $var (mul 32 $ind)) $val)"
    },
    {
        "(getch $var $ind)",
        "(mod (mload (add $var $ind)) 256)"
    },
    {
        "(setch $var $ind $val)",
        "(mstore8 (add $var $ind) $val)",
    },
    {
        "(send $to $value)",
        "(~call (sub (gas) 25) $to $value 0 0 0 0)"
    },
    {
        "(send $gas $to $value)",
        "(~call $gas $to $value 0 0 0 0)"
    },
    {
        "(sha3 $x)",
        "(seq (set $1 $x) (~sha3 (ref $1) 32))"
    },
    {
        "(sha3 $mstart (= chars $msize))",
        "(~sha3 $mstart $msize)"
    },
    {
        "(sha3 $mstart $msize)",
        "(~sha3 $mstart (mul 32 $msize))"
    },
    {
        "(id $0)",
        "$0"
    },
    {
        "(return $x)",
        "(seq (set $1 $x) (~return (ref $1) 32))"
    },
    {
        "(return $mstart (= chars $msize))",
        "(~return $mstart $msize)"
    },
    {
        "(return $start $len)",
        "(~return $start (mul 32 $len))"
    },
    {
        "(&& $x $y)",
        "(if $x $y 0)"
    },
    {
        "(|| $x $y)",
        "(if (get $x) (get $x) $y)"
    },
    {
        "(>= $x $y)",
        "(iszero (slt $x $y))"
    },
    {
        "(<= $x $y)",
        "(iszero (sgt $x $y))"
    },
    {
        "(@>= $x $y)",
        "(iszero (lt $x $y))"
    },
    {
        "(@<= $x $y)",
        "(iszero (gt $x $y))"
    },
    {
        "(create $code)",
        "(create 0 $code)"
    },
    {
        "(create $endowment $code)",
        "(with $1 (msize) (create $endowment (get $1) (lll (outer $code) (msize))))"
    },
    {
        "(sha256 $x)",
        "(seq (set $1 $x) (pop (~call 101 2 0 (ref $1) 32 (ref $2) 32)) (get $2))"
    },
    {
        "(sha256 $arr $sz)",
        "(seq (pop (~call 101 2 0 $arr (mul 32 $sz) (ref $2) 32)) (get $2))"
    },
    {
        "(ripemd160 $x)",
        "(seq (set $1 $x) (pop (~call 101 3 0 (ref $1) 32 (ref $2) 32)) (get $2))"
    },
    {
        "(ripemd160 $arr $sz)",
        "(seq (pop (~call 101 3 0 $arr (mul 32 $sz) (ref $2) 32)) (get $2))"
    },
    {
        "(ecrecover $h $v $r $s)",
        "(seq (declare $1) (declare $2) (declare $3) (declare $4) (set $1 $h) (set $2 $v) (set $3 $r) (set $4 $s) (pop (~call 101 1 0 (ref $1) 128 (ref $5) 32)) (get $5))"
    },
    {
        "(seq (seq) $x)",
        "$x"
    },
    {
        "(inset $x)",
        "$x"
    },
    {
        "(create $x)",
        "(with $1 (msize) (create $val (get $1) (lll $code (get $1))))"
    },
    {
        "(with (= $var $val) $cond)",
        "(with $var $val $cond)"
    },
    {
        "(log $t1)",
        "(~log1 0 0 $t1)"
    },
    {
        "(log $t1 $t2)",
        "(~log2 0 0 $t1 $t2)"
    },
    {
        "(log $t1 $t2 $t3)",
        "(~log3 0 0 $t1 $t2 $t3)"
    },
    {
        "(log $t1 $t2 $t3 $t4)",
        "(~log4 0 0 $t1 $t2 $t3 $t4)"
    },
    {
        "(save $loc $array (= chars $count))",
        "(with $location (ref $loc) (with $c $count (with $end (div $c 32) (with $i 0 (seq (while (slt $i $end) (seq (sstore (add $i $location) (access $array $i)) (set $i (add $i 1)))) (sstore (add $i $location) (~and (access $array $i) (sub 0 (exp 256 (sub 32 (mod $c 32)))))))))))"
    },
    {
        "(save $loc $array $count)",
        "(with $location (ref $loc) (with $end $count (with $i 0 (while (slt $i $end) (seq (sstore (add $i $location) (access $array $i)) (set $i (add $i 1)))))))"
    },
    {
        "(load $loc (= chars $count))",
        "(with $location (ref $loc) (with $c $count (with $a (alloc $c) (with $i 0 (seq (while (slt $i (div $c 32)) (seq (set (access $a $i) (sload (add $location $i))) (set $i (add $i 1)))) (set (access $a $i) (~and (sload (add $location $i)) (sub 0 (exp 256 (sub 32 (mod $c 32)))))) $a)))))"
    },
    {
        "(load $loc $count)",
        "(with $location (ref $loc) (with $c $count (with $a (alloc $c) (with $i 0 (seq (while (slt $i $c) (seq (set (access $a $i) (sload (add $location $i))) (set $i (add $i 1)))) $a)))))"
    },
    {
        "(unsafe_mcopy $to $from $sz)",
        "(seq (comment STARTING GHETTO UNSAFE_MCOPY) (set $i 0) (while (lt $i $sz) (seq (mstore (add $to $i) (mload (add $from $i))) (set $i (add $i 32)))))"
    },
    {
        "(mcopy $to $from $sz)",
        "(seq (comment STARTING GHETTO MCOPY) (set $i 0) (while (lt (add $i 31) $sz) (seq (mstore (add $to $i) (mload (add $from $i))) (set $i (add $i 32)))) (set $mask (exp 256 (sub 32 (mod $sz 32)))) (mstore (add $to $i) (add (mod (mload (add $to $i)) $mask) (and (mload (add $from $i)) (sub 0 $mask)))))"
    },
    { "(. msg datasize)", "(div (calldatasize) 32)" },
    { "(. msg sender)", "(caller)" },
    { "(. msg value)", "(callvalue)" },
    { "(. tx gasprice)", "(gasprice)" },
    { "(. tx origin)", "(origin)" },
    { "(. tx gas)", "(gas)" },
    { "(. $x balance)", "(balance $x)" },
    { "self", "(address)" },
    { "(. block prevhash)", "(prevhash)" },
    { "(. block coinbase)", "(coinbase)" },
    { "(. block timestamp)", "(timestamp)" },
    { "(. block number)", "(number)" },
    { "(. block difficulty)", "(difficulty)" },
    { "(. block gaslimit)", "(gaslimit)" },
    { "stop", "(stop)" },
    { "---END---", "" } //Keep this line at the end of the list
};

std::vector<std::vector<Node> > nodeMacros;

std::string synonyms[][2] = {
    { "or", "||" },
    { "and", "&&" },
    { "|", "~or" },
    { "&", "~and" },
    { "elif", "if" },
    { "!", "iszero" },
    { "~", "~not" },
    { "not", "iszero" },
    { "string", "alloc" },
    { "+", "add" },
    { "-", "sub" },
    { "*", "mul" },
    { "/", "sdiv" },
    { "^", "exp" },
    { "**", "exp" },
    { "%", "smod" },
    { "@/", "div" },
    { "@%", "mod" },
    { "@<", "lt" },
    { "@>", "gt" },
    { "<", "slt" },
    { ">", "sgt" },
    { "=", "set" },
    { "==", "eq" },
    { ":", "kv" },
    { "---END---", "" } //Keep this line at the end of the list
};

std::string setters[][2] = {
    { "+=", "+" },
    { "-=", "-" },
    { "*=", "*" },
    { "/=", "/" },
    { "%=", "%" },
    { "^=", "^" },
    { "---END---", "" } //Keep this line at the end of the list
};

// Processes mutable array literals

Node array_lit_transform(Node node) {
    std::string prefix = "_temp"+mkUniqueToken() + "_";
    Metadata m = node.metadata;
    std::map<std::string, Node> d;
    std::string o = "(seq (set $arr (alloc "+utd(node.args.size()*32)+"))";
    for (unsigned i = 0; i < node.args.size(); i++) {
        o += " (mstore (add (get $arr) "+utd(i * 32)+") $"+utd(i)+")";
        d[utd(i)] = node.args[i];
    }
    o += " (get $arr))";
    return subst(parseLLL(o), d, prefix, m);
}


Node apply_rules(preprocessResult pr);

// Transform a node of the form (call to funid vars...) into
// a call

#define psn std::pair<std::string, Node>

Node call_transform(Node node, std::string op) {
    Metadata m = node.metadata;
    // We're gonna make lots of temporary variables,
    // so set up a unique flag for them
    std::string prefix = "_temp"+mkUniqueToken()+"_";
    // kwargs = map of special arguments
    std::map<std::string, Node> kwargs;
    kwargs["value"] = token("0", m);
    kwargs["gas"] = subst(parseLLL("(- (gas) 25)"), msn(), prefix, m);
    std::vector<Node> args;
    std::vector<Node> szargs;
    std::vector<Node> vlargs;
    for (unsigned i = 0; i < node.args.size(); i++) {
        // Parameter (eg. gas, value)
        if (node.args[i].val == "=" || node.args[i].val == "set") {
            if (node.args[i].args.size() != 2)
                err("Malformed set", m);
            kwargs[node.args[i].args[0].val] = node.args[i].args[1];
        }
        // Sized argument
        else if (node.args[i].val == ":") {
            szargs.push_back(node.args[i].args[1]);
            vlargs.push_back(node.args[i].args[0]);
        }
        // Regular argument
        else args.push_back(node.args[i]);
    }
    if (args.size() < 2) err("Too few arguments for call!", m);
    kwargs["to"] = args[0];
    kwargs["funid"] = args[1];
    std::vector<Node> inputs;
    for (unsigned i = 2; i < args.size(); i++) {
        inputs.push_back(args[i]);
    }
    std::vector<Node> pre;
    std::vector<Node> post;
    if (szargs.size() == 0) {
        // Here, there is no data array, instead there are function arguments.
        // This actually lets us be much more efficient with how we set things
        // up.
        // Pre-declare variables; relies on declared variables being sequential
        std::vector<Node> declareVars;
        declareVars.push_back(token(prefix+"prebyte", m));
        for (unsigned i = 0; i < inputs.size(); i++)
            declareVars.push_back(token(prefix+utd(i), m));
        pre.push_back(astnode("declare", declareVars, m));
        std::string pattern =
          + "    (seq                                                   "
            "        (set $datastart (add 31 (ref "+prefix+"prebyte)))  "
          + "        (mstore8 $datastart $funid)                        ";
        for (unsigned i = 0; i < inputs.size(); i++) {
            pattern +=
                "    (set "+prefix+utd(i)+" $"+utd(i)+")                ";
            kwargs[utd(i)] = inputs[i];
                
        }
        pattern += "))";
        pre.push_back(parseLLL(pattern));
        kwargs["datain"] = token(prefix+"datastart", m);
        kwargs["datainsz"] = token(utd(inputs.size()*32+1), m);
    }
    else {
        // Start off by saving the size variables and calculating the total
        std::string pattern =
            "(with $sztot 0 (seq";
        for (unsigned i = 0; i < szargs.size(); i++) {
            pattern +=
                "(set $var"+utd(i)+" (alloc $sz"+utd(i)+"))                   "
              + "(set $sztot (add $sztot $sz"+utd(i)+"))                      ";
            kwargs["sz"+utd(i)] = szargs[i];
        }
        int startpos = 1 + (szargs.size() + inputs.size()) * 32;
        pattern +=
                "(set $datastart (alloc (add "+utd(startpos+32)+" $sztot)))   "
              + "(mstore8 $datastart $funid)                                  ";
        for (unsigned i = 0; i < szargs.size(); i++) {
            pattern +=
                "(mstore (add $datastart "+utd(1 + i * 32)+") $sz"+utd(i)+")  ";
        }
        for (unsigned i = 0; i < inputs.size(); i++) {
            int v = 1 + (i + szargs.size()) * 32;
            pattern +=
                "(mstore (add $datastart "+utd(v)+") $"+utd(i)+")             ";
            kwargs[utd(i)] = inputs[i];
        }
        pattern += 
                "(set $pos (add $datastart "+utd(startpos)+"))                ";
        for (unsigned i = 0; i < vlargs.size(); i++) {
            pattern +=
                "(unsafe_mcopy $pos $vl"+utd(i)+" $sz"+utd(i)+")              ";
            kwargs["vl"+utd(i)] = vlargs[i];
        }
        pattern += "))";
        pre.push_back(parseLLL(pattern));
        kwargs["datain"] = token(prefix+"datastart", m);
        kwargs["datainsz"] = astnode("add",
                                     token(utd(startpos), m),
                                     token(prefix+"sztot", m),
                                     m);
    }
    if (!kwargs.count("outsz")) {
        kwargs["dataout"] = astnode("ref", token(prefix+"dataout", m), m);
        kwargs["dataoutsz"] = token("32", node.metadata);
        post.push_back(astnode("get", token(prefix+"dataout", m), m));
    }
    else {
        kwargs["dataoutsz"]
            = astnode("mul", token("32", m), kwargs["outsz"], m);
        kwargs["dataout"] 
            = astnode("alloc", token(prefix+"dataoutsz", m), m);
        post.push_back(astnode("get", token(prefix+"dataout", m), m));
    }
    // Set up main call
    std::vector<Node> main;
    for (unsigned i = 0; i < pre.size(); i++) {
        main.push_back(pre[i]);
    }
    main.push_back(parseLLL(
        "(pop (~"+op+" $gas $to $value $datain $datainsz $dataout $dataoutsz))"));
    for (unsigned i = 0; i < post.size(); i++) {
        main.push_back(post[i]);
    }
    Node o = subst(astnode("seq", main, m), kwargs, prefix, m);
    return o;
}





// Transform "<variable>.<fun>(args...)" into
// (call <variable> <funid> args...)
Node dotTransform(Node node, preprocessAux aux) {
    Metadata m = node.metadata;
    Node pre = node.args[0].args[0];
    std::string post = node.args[0].args[1].val;
    if (node.args[0].args[1].type == ASTNODE)
        err("Function name must be static", m);
    // Search for as=? and call=code keywords
    std::string as = "";
    bool call_code = false;
    for (unsigned i = 1; i < node.args.size(); i++) {
        Node arg = node.args[i];
        if (arg.val == "=" || arg.val == "set") {
            if (arg.args[0].val == "as")
                as = arg.args[1].val;
            if (arg.args[0].val == "call" && arg.args[1].val == "code")
                call_code = true;
        }
    }
    if (pre.val == "self") {
        if (as.size()) err("Cannot use \"as\" when calling self!", m);
        as = pre.val;
    }
    std::vector<Node> args;
    args.push_back(pre);
    std::string sig;
    // Determine the funId and sig assuming the "as" keyword was used
    if (as.size() > 0 && aux.localExterns.count(as)) {
        if (!aux.localExterns[as].count(post))
            err("Invalid call: "+printSimple(pre)+"."+post, m);
        std::string funid = unsignedToDecimal(aux.localExterns[as][post]);
        sig = aux.localExternSigs[as][post];
        args.push_back(token(funid, m));
    }
    // Determine the funId and sig otherwise
    else if (!as.size()) {
        if (!aux.globalExterns.count(post))
            err("Invalid call: "+printSimple(pre)+"."+post, m);
        std::string key = unsignedToDecimal(aux.globalExterns[post]);
        sig = aux.globalExternSigs[post];
        args.push_back(token(key, m));
    }
    else err("Invalid call: "+printSimple(pre)+"."+post, m);
    unsigned argCount = 0;
    for (unsigned i = 1; i < node.args.size(); i++) {
        if (node.args[i].val == "=")
            args.push_back(node.args[i]);
        else {
            char argType;
            if (sig.size() > 0) {
                if (argCount >= sig.size())
                    err("Too many args", m);
                argType = sig[argCount];
            }
            else argType = 'i';
            if (argType == 'i') {
                if (node.args[i].val == ":")
                    err("Function asks for int, provided string or array", m);
                args.push_back(node.args[i]);
            }
            else if (argType == 's') {
                if (node.args[i].val != ":")
                    err("Must specify string length", m);
                args.push_back(node.args[i]);
            }
            else if (argType == 'a') {
                if (node.args[i].val != ":")
                    err("Must specify array length", m);
                args.push_back(astnode(":",
                                       node.args[i].args[0],
                                       astnode("mul",
                                               node.args[i].args[1],
                                               token("32", m),
                                               m),
                                       m));
            }
            else err("Invalid arg type in signature", m);
            argCount++;
        }
    }
    return astnode(call_code ? "call_code" : "call", args, m);
}

// Transform an access of the form self.bob, self.users[5], etc into
// a storage access
//
// There exist two types of objects: finite objects, and infinite
// objects. Finite objects are packed optimally tightly into storage
// accesses; for example:
//
// data obj[100](a, b[2][4], c)
//
// obj[0].a -> 0
// obj[0].b[0][0] -> 1
// obj[0].b[1][3] -> 8
// obj[45].c -> 459
//
// Infinite objects are accessed by sha3([v1, v2, v3 ... ]), where
// the values are a list of array indices and keyword indices, for
// example:
// data obj[](a, b[2][4], c)
// data obj2[](a, b[][], c)
//
// obj[0].a -> sha3([0, 0, 0])
// obj[5].b[1][3] -> sha3([0, 5, 1, 1, 3])
// obj[45].c -> sha3([0, 45, 2])
// obj2[0].a -> sha3([1, 0, 0])
// obj2[5].b[1][3] -> sha3([1, 5, 1, 1, 3])
// obj2[45].c -> sha3([1, 45, 2])
Node storageTransform(Node node, preprocessAux aux,
                      bool mapstyle=false, bool ref=false) {
    Metadata m = node.metadata;
    // Get a list of all of the "access parameters" used in order
    // eg. self.users[5].cow[4][m[2]][woof] -> 
    //         [--self, --users, 5, --cow, 4, m[2], woof]
    std::vector<Node> hlist = listfyStorageAccess(node);
    // For infinite arrays, the terms array will just provide a list
    // of indices. For finite arrays, it's a list of index*coefficient
    std::vector<Node> terms;
    std::string offset = "0";
    std::string prefix = "";
    std::string varPrefix = "_temp"+mkUniqueToken()+"_";
    int c = 0;
    std::vector<std::string> coefficients;
    coefficients.push_back("");
    for (unsigned i = 1; i < hlist.size(); i++) {
        // We pre-add the -- flag to parameter-like terms. For example,
        // self.users[m] -> [--self, --users, m]
        // self.users.m -> [--self, --users, --m]
        if (hlist[i].val.substr(0, 2) == "--") {
            prefix += hlist[i].val.substr(2) + ".";
            std::string tempPrefix = prefix.substr(0, prefix.size()-1);
            if (!aux.storageVars.offsets.count(tempPrefix))
                return node;
            if (c < (signed)coefficients.size() - 1)
                err("Too few array index lookups", m);
            if (c > (signed)coefficients.size() - 1)
                err("Too many array index lookups", m);
            coefficients = aux.storageVars.coefficients[tempPrefix];
            // If the size of an object exceeds 2^176, we make it an infinite
            // array
            if (decimalGt(coefficients.back(), tt176) && !mapstyle)
                return storageTransform(node, aux, true, ref);
            offset = decimalAdd(offset, aux.storageVars.offsets[tempPrefix]);
            c = 0;
            if (mapstyle)
                terms.push_back(token(unsignedToDecimal(
                    aux.storageVars.indices[tempPrefix])));
        }
        else if (mapstyle) {
            terms.push_back(hlist[i]);
            c += 1;
        }
        else {
            if (c > (signed)coefficients.size() - 2)
                err("Too many array index lookups", m);
            terms.push_back(
                astnode("mul", 
                        hlist[i],
                        token(coefficients[coefficients.size() - 2 - c], m),
                        m));
                                    
            c += 1;
        }
    }
    if (aux.storageVars.nonfinal.count(prefix.substr(0, prefix.size()-1)))
        err("Storage variable access not deep enough", m);
    if (c < (signed)coefficients.size() - 1) {
        err("Too few array index lookups", m);
    }
    if (c > (signed)coefficients.size() - 1) {
        err("Too many array index lookups", m);
    }
    Node o;
    if (mapstyle) {
        // We pre-declare variables, relying on the idea that sequentially
        // declared variables are doing to appear beside each other in
        // memory
        std::vector<Node> main;
        for (unsigned i = 0; i < terms.size(); i++)
            main.push_back(astnode("declare",
                                   token(varPrefix+unsignedToDecimal(i), m),
                                   m));
        for (unsigned i = 0; i < terms.size(); i++)
            main.push_back(astnode("set",
                                   token(varPrefix+unsignedToDecimal(i), m),
                                   terms[i],
                                   m));
        main.push_back(astnode("ref", token(varPrefix+"0", m), m));
        Node sz = token(unsignedToDecimal(terms.size()), m);
        o = astnode("sha3",
                    astnode("seq", main, m),
                    sz,
                    m);
    }
    else {
        // We add up all the index*coefficients
        Node out = token(offset, node.metadata);
        for (unsigned i = 0; i < terms.size(); i++) {
            std::vector<Node> temp;
            temp.push_back(out);
            temp.push_back(terms[i]);
            out = astnode("add", temp, node.metadata);
        }
        o = out;
    }
    if (ref) return o;
    else return astnode("sload", o, node.metadata);
}


// Recursively applies rewrite rules
Node apply_rules(preprocessResult pr) {
    Node node = pr.first;
    // If the rewrite rules have not yet been parsed, parse them
    if (!nodeMacros.size()) {
        for (int i = 0; i < 9999; i++) {
            std::vector<Node> o;
            if (macros[i][0] == "---END---") break;
            o.push_back(parseLLL(macros[i][0]));
            o.push_back(parseLLL(macros[i][1]));
            nodeMacros.push_back(o);
        }
    }
    // Assignment transformations
    for (int i = 0; i < 9999; i++) {
        if (setters[i][0] == "---END---") break;
        if (node.val == setters[i][0]) {
            node = astnode("=",
                           node.args[0],
                           astnode(setters[i][1],
                                   node.args[0],
                                   node.args[1],
                                   node.metadata),
                           node.metadata);
        }
    }
    // Special storage transformation
    if (node.val == "comment") {
        return node;
    }
    if (isNodeStorageVariable(node)) {
        node = storageTransform(node, pr.second);
    }
    if (node.val == "ref" && isNodeStorageVariable(node.args[0])) {
        node = storageTransform(node.args[0], pr.second, false, true);
    }
    if (node.val == "=" && isNodeStorageVariable(node.args[0])) {
        Node t = storageTransform(node.args[0], pr.second);
        if (t.val == "sload") {
            std::vector<Node> o;
            o.push_back(t.args[0]);
            o.push_back(node.args[1]);
            node = astnode("sstore", o, node.metadata);
        }
    }
    // Main code
	unsigned pos = 0;
    std::string prefix = "_temp"+mkUniqueToken()+"_";
    while(1) {
        if (synonyms[pos][0] == "---END---") {
            break;
        }
        else if (node.type == ASTNODE && node.val == synonyms[pos][0]) {
            node.val = synonyms[pos][1];
        }
        pos++;
    }
    for (pos = 0; pos < nodeMacros.size(); pos++) {
        Node pattern = nodeMacros[pos][0];
        matchResult mr = match(pattern, node);
        if (mr.success) {
            Node pattern2 = nodeMacros[pos][1];
            node = subst(pattern2, mr.map, prefix, node.metadata);
            pos = 0;
        }
    }
    // Special transformations
    if (node.val == "outer") {
        pr = preprocess(node);
        node = pr.first;
    }
    if (node.val == "array_lit")
        node = array_lit_transform(node);
    if (node.val == "fun" && node.args[0].val == ".") {
        node = dotTransform(node, pr.second);
    }
    if (node.val == "call")
        node = call_transform(node, "call");
    if (node.val == "call_code")
        node = call_transform(node, "call_code");
    if (node.type == ASTNODE) {
		unsigned i = 0;
        if (node.val == "set" || node.val == "ref" 
                || node.val == "get" || node.val == "with") {
            node.args[0].val = "'" + node.args[0].val;
            i = 1;
        }
        if (node.val == "getlen") {
            node.val = "get";
            node.args[0].val = "'_len_" + node.args[0].val;
            i = 1;
        }
        if (node.val == "declare") {
            for (; i < node.args.size(); i++) {
                node.args[i].val = "'" + node.args[i].val;
            }
        }
        for (; i < node.args.size(); i++) {
            node.args[i] =
                apply_rules(preprocessResult(node.args[i], pr.second));
        }
    }
    else if (node.type == TOKEN && !isNumberLike(node)) {
        if (node.val.size() >= 2
                && node.val[0] == '"'
                && node.val[node.val.size() - 1] == '"') {
            std::string bin = node.val.substr(1, node.val.size() - 2);
            unsigned sz = bin.size();
            std::vector<Node> o;
            for (unsigned i = 0; i < sz; i += 32) {
                std::string t = binToNumeric(bin.substr(i, 32));
                if ((sz - i) < 32 && (sz - i) > 0) {
                    while ((sz - i) < 32) {
                        t = decimalMul(t, "256");
                        i--;
                    }
                    i = sz;
                }
                o.push_back(token(t, node.metadata));
            }
            node = astnode("array_lit", o, node.metadata);
        }
        else {
            node.val = "'" + node.val;
            std::vector<Node> args;
            args.push_back(node);
            node = astnode("get", args, node.metadata);
        }
    }
    // This allows people to use ~x as a way of having functions with the same
    // name and arity as macros; the idea is that ~x is a "final" form, and 
    // should not be remacroed, but it is converted back at the end
    if (node.type == ASTNODE && node.val[0] == '~')
        node.val = node.val.substr(1);
    return node;
}

Node validate(Node inp) {
    if (inp.type == ASTNODE) {
        int i = 0;
        while(valid[i][0] != "---END---") {
            if (inp.val == valid[i][0]) {
                std::string sz = unsignedToDecimal(inp.args.size());
                if (decimalGt(valid[i][1], sz)) {
                    err("Too few arguments for "+inp.val, inp.metadata);   
                }
                if (decimalGt(sz, valid[i][2])) {
                    err("Too many arguments for "+inp.val, inp.metadata);   
                }
            }
            i++;
        }
    }
	for (unsigned i = 0; i < inp.args.size(); i++) validate(inp.args[i]);
    return inp;
}

Node postValidate(Node inp) {
    if (inp.type == ASTNODE) {
        if (inp.val == ".")
            err("Invalid object member (ie. a foo.bar not mapped to anything)",
                inp.metadata);
        for (unsigned i = 0; i < inp.args.size(); i++)
            postValidate(inp.args[i]);
    }
    return inp;
}

Node outerWrap(Node inp) {
    return astnode("outer", inp, inp.metadata);
}

Node rewrite(Node inp) {
    return postValidate(optimize(apply_rules(preprocessResult(
                validate(outerWrap(inp)), preprocessAux()))));
}

Node rewriteChunk(Node inp) {
    return postValidate(optimize(apply_rules(preprocessResult(
                validate(inp), preprocessAux()))));
}

using namespace std;
