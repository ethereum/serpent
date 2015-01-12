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
#include "opcodes.h"

// Rewrite rules
std::string macros[][2] = {
    {
        "(seq $x)",
        "$x"
    },
    {
        "(seq (seq) $x)",
        "$x"
    },
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
        "(with $1 $a (with $2 $b (if (lt $1 $2) $1 $2)))"
    },
    {
        "(max $a $b)",
        "(with $1 $a (with $2 $b (if (lt $1 $2) $2 $1)))"
    },
    {
        "(smin $a $b)",
        "(with $1 $a (with $2 $b (if (slt $1 $2) $1 $2)))"
    },
    {
        "(smax $a $b)",
        "(with $1 $a (with $2 $b (if (slt $1 $2) $2 $1)))"
    },
    {
        "(if $cond $do (else $else))",
        "(if $cond $do $else)"
    },
    {
        "(code $code)",
        "$code"
    },
    {
        "(array $len)",
        "(with $l $len (with $x (alloc (add 32 (mul 32 $l))) (seq (mstore $x $l) (add $x 32))))"
    },
    {
        "(string $len)",
        "(with $l $len (with $x (alloc (add 32 $l)) (seq (mstore $x $l) (add $x 32))))"
    },
    {
        "(shrink $arr $sz)",
        "(mstore (sub $arr 32) $sz)"
    },
    {
        "(len $x)",
        "(mload (sub $x 32))"
    },
    {
        "(return (: $x s))",
        "(with $y $x (~return $y (len $y)))"
    },
    {
        "(return (: $x a))",
        "(with $y $x (~return $y (mul 32 (len $y))))"
    },
    {
        "(while $cond $do)",
        "(until (iszero $cond) $do)",
    },
    {
        "(while (iszero $cond) $do)",
        "(until $cond $do)",
    },
    {
        "(if $cond $do)",
        "(unless (iszero $cond) $do)",
    },
    {
        "(if (iszero $cond) $do)",
        "(unless $cond $do)",
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
        "(set (sload $ind) $val)",
        "(sstore $ind $val)"
    },
    {
        "(set (access $var $ind) $val)",
        "(mstore (add $var (mul 32 $ind)) $val)"
    },
    {
        "(getch $var $ind)",
        "(mod (mload (sub (add $var $ind) 31)) 256)"
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
        "(with $1 $x (if $1 $1 $y))"
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
        "(create $code)",
        "(create 0 $code)"
    },
    {
        "(create $endowment $code)",
        "(with $1 (msize) (create $endowment (get $1) (lll $code (msize))))"
    },
    {
        "(sha256 (: $x a))",
        "(with $0 $x (sha256 $0 (mload (sub $0 32))))"
    },
    {
        "(sha256 (: $x s))",
        "(with $0 $x (sha256 $0 (= chars (mload (sub $0 32)))))"
    },
    {
        "(sha256 $x)",
        "(with $1 (alloc 64) (seq (mstore (add (get $1) 32) $x) (pop (~call 101 2 0 (add (get $1) 32) 32 (get $1) 32)) (mload (get $1))))"
    },
    {
        "(sha256 $arr (= chars $sz))",
        "(with $0 $sz (with $1 (alloc 32) (seq (pop (~call (add 101 (mul 2 $0)) 2 0 $arr $0 (get $1) 32)) (mload (get $1)))))"
    },
    {
        "(sha256 $arr $sz)",
        "(with $0 $sz (with $1 (alloc 32) (seq (pop (~call (add 101 (mul 50 $0)) 2 0 $arr (mul 32 $0) (get $1) 32)) (mload (get $1)))))"
    },
    {
        "(ripemd160 (: $x a))",
        "(with $0 $x (ripemd160 $0 (mload (sub $0 32))))"
    },
    {
        "(ripemd160 (: $x s))",
        "(with $0 $x (ripemd160 $0 (= chars (mload (sub $0 32)))))"
    },
    {
        "(ripemd160 $x)",
        "(with $1 (alloc 64) (seq (mstore (add (get $1) 32) $x) (pop (~call 101 3 0 (add (get $1) 32) 32 (get $1) 32)) (mload (get $1))))"
    },
    {
        "(ripemd160 $arr (= chars $sz))",
        "(with $0 $sz (with $1 (alloc 32) (seq (pop (~call (add 101 (mul 2 $0)) 3 0 $arr $0 (get $1) 32)) (mload (get $1)))))"
    },
    {
        "(ripemd160 $arr $sz)",
        "(with $0 $sz (with $1 (alloc 32) (seq (pop (~call (add 101 (mul 50 $0)) 3 0 $arr (mul 32 $0) (get $1) 32)) (mload (get $1)))))"
    },
    {
        "(ecrecover $h $v $r $s)",
        "(with $1 (alloc 160) (seq (mstore (get $1) $h) (mstore (add (get $1) 32) $v) (mstore (add (get $1) 64) $r) (mstore (add (get $1) 96) $s) (pop (~call 101 1 0 (get $1) 128 (add (get $1 128)) 32)) (mload (add (get $1) 128))))"
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
        "(with _sz $sz (with _from $from (with _to $to (seq (comment STARTING UNSAFE MCOPY) (with _i 0 (while (lt _i _sz) (seq (mstore (add $to _i) (mload (add _from _i))) (set _i (add _i 32)))))))))"
    },
    {
        "(mcopy $to $from $_sz)",
        "(with _to $to (with _from $from (with _sz $sz (seq (comment STARTING MCOPY (with _i 0 (seq (while (lt (add _i 31) _sz) (seq (mstore (add _to _i) (mload (add _from _i))) (set _i (add _i 32)))) (with _mask (exp 256 (sub 32 (mod _sz 32))) (mstore (add $to _i) (add (mod (mload (add $to _i)) _mask) (and (mload (add $from _i)) (sub 0 _mask))))))))))))"
    },
    { "(. msg sender)", "(caller)" },
    { "(. msg value)", "(callvalue)" },
    { "(. tx gasprice)", "(gasprice)" },
    { "(. tx origin)", "(origin)" },
    { "(. tx gas)", "(gas)" },
    { "(. $x balance)", "(balance $x)" },
    { "self", "(address)" },
    { "(. block prevhash)", "(blockhash (sub (number) 1))" },
    { "(fun (. block prevhash) $n)", "(blockhash (sub (number) $n))" },
    { "(. block coinbase)", "(coinbase)" },
    { "(. block timestamp)", "(timestamp)" },
    { "(. block number)", "(number)" },
    { "(. block difficulty)", "(difficulty)" },
    { "(. block gaslimit)", "(gaslimit)" },
    { "stop", "(stop)" },
    { "---END---", "" } //Keep this line at the end of the list
};

// Token synonyms
std::string synonyms[][2] = {
    { "or", "||" },
    { "and", "&&" },
    { "|", "~or" },
    { "&", "~and" },
    { "elif", "if" },
    { "!", "iszero" },
    { "~", "~not" },
    { "not", "iszero" },
    { "+", "add" },
    { "-", "sub" },
    { "*", "mul" },
    { "/", "sdiv" },
    { "^", "exp" },
    { "**", "exp" },
    { "%", "smod" },
    { "<", "slt" },
    { ">", "sgt" },
    { "=", "set" },
    { "==", "eq" },
    { ":", "kv" },
    { "---END---", "" } //Keep this line at the end of the list
};

std::map<std::string, std::string> synonymMap;

// Custom setters (need to be registered separately
// for use with managed storage)
std::string setters[][2] = {
    { "+=", "+" },
    { "-=", "-" },
    { "*=", "*" },
    { "/=", "/" },
    { "%=", "%" },
    { "^=", "^" },
    { "---END---", "" } //Keep this line at the end of the list
};

std::map<std::string, std::string> setterMap;


// processes mutable array literals
Node array_lit_transform(Node node) {
    std::string prefix = "_temp"+mkUniqueToken() + "_";
    Metadata m = node.metadata;
    std::map<std::string, Node> d;
    std::string o = "(with $arr (alloc "+utd(node.args.size()*32+32)+")";
    o +=     " (seq (mstore (get $arr) "+utd(node.args.size())+")";
    for (unsigned i = 0; i < node.args.size(); i++) {
        o += " (mstore (add (get $arr) "+utd(i * 32 + 32)+") $"+utd(i)+")";
        d[utd(i)] = node.args[i];
    }
    o += " (add (get $arr) 32)))";
    return subst(parseLLL(o), d, prefix, m);
}

Node log_transform(Node node) {
    Metadata m = node.metadata;
    std::vector<Node> topics;
    Node data;
    bool usingData = false;
    bool isDataString = false;
    for (unsigned i = 0; i < node.args.size(); i++) {
        if (node.args[i].val == "=") {
            std::string v = node.args[i].args[0].val;
            if (v == "data" || v == "datastr") {
                data = node.args[i].args[1];
                usingData = true;
                isDataString = (v == "datastr");
            }
        }
        else topics.push_back(node.args[i]);
    }
    if (topics.size() > 4) err("Too many topics!", m);
    std::vector<Node> out;
    if (usingData) {
        out.push_back(tkn("_datarr", m));
        out.push_back(isDataString
            ? asn("len", tkn("_datarr", m), m)
            : asn("mul", tkn("32", m), asn("len", tkn("_datarr", m), m)));
    }
    else {
        out.push_back(tkn("0", m));
        out.push_back(tkn("0", m));
    }
    std::string t = utd(topics.size());
    for (unsigned i = 0; i < topics.size(); i++)
        out.push_back(topics[i]);
    if (usingData)
        return asn("with", tkn("_datarr", m), data, asn("~log"+t, out, m), m);
    else
        return asn("~log"+t, out, m);
}
 

// Processes long text literals
Node string_transform(Node node) {
    std::string prefix = "_temp"+mkUniqueToken() + "_";
    Metadata m = node.metadata;
    if (!node.args.size())
        err("Empty text!", m);
    if (node.args[0].val.size() < 2 
     || node.args[0].val[0] != '"'
     || node.args[0].val[node.args[0].val.size() - 1] != '"')
        err("Text contents don't look like a string!", m);
    std::string bin = node.args[0].val.substr(1, node.args[0].val.size() - 2);
    unsigned sz = bin.size();
    std::map<std::string, Node> d;
    std::string o = "(with $str (alloc "+utd(bin.size() + 32)+")";
    o += " (seq (mstore (get $str) "+utd(bin.size())+")";
    for (unsigned i = 0; i < sz; i += 32) {
        unsigned curpos = i;
        std::string t = binToNumeric(bin.substr(i, 32));
        if ((sz - i) < 32 && (sz - i) > 0) {
            while ((sz - i) < 32) {
                t = decimalMul(t, "256");
                i--;
            }
            i = sz;
        }
        o += " (mstore (add (get $str) "+utd(curpos + 32)+") "+t+")";
    }
    o += " (add (get $str) 32)))";
    return subst(parseLLL(o), d, prefix, m);
}


Node apply_rules(preprocessResult pr);

// Transform "<variable>.<fun>(args...)" into
// a call
Node dotTransform(Node node, preprocessAux aux) {
    Metadata m = node.metadata;
    // We're gonna make lots of temporary variables,
    // so set up a unique flag for them
    std::string prefix = "_temp"+mkUniqueToken()+"_";
    // Check that the function name is a token
    if (node.args[0].args[1].type == ASTNODE)
        err("Function name must be static", m);

    Node dotOwner = node.args[0].args[0];
    std::string dotMember = node.args[0].args[1].val;
    // kwargs = map of special arguments
    std::map<std::string, Node> kwargs;
    kwargs["value"] = token("0", m);
    kwargs["gas"] = subst(parseLLL("(- (gas) 25)"), msn(), prefix, m);
    // Search for as=? and call=code keywords, and isolate the actual
    // function arguments
    std::vector<Node> fnargs;
    std::string as = "";
    std::string op = "call";
    for (unsigned i = 1; i < node.args.size(); i++) {
        fnargs.push_back(node.args[i]);
        Node arg = fnargs.back();
        if (arg.val == "=" || arg.val == "set") {
            if (arg.args[0].val == "as")
                as = arg.args[1].val;
            if (arg.args[0].val == "call" && arg.args[1].val == "code")
                op = "callcode";
            if (arg.args[0].val == "gas")
                kwargs["gas"] = arg.args[1];
            if (arg.args[0].val == "value")
                kwargs["value"] = arg.args[1];
            if (arg.args[0].val == "outsz")
                kwargs["outsz"] = arg.args[1];
        }
    }
    if (dotOwner.val == "self") {
        if (as.size()) err("Cannot use \"as\" when calling self!", m);
        as = dotOwner.val;
    }
    // Determine the funId and sig assuming the "as" keyword was used
    int funId = 0;
    std::string sig;
    if (as.size() > 0 && aux.localExterns.count(as)) {
        if (!aux.localExterns[as].count(dotMember))
            err("Invalid call: "+printSimple(dotOwner)+"."+dotMember, m);
        funId = aux.localExterns[as][dotMember];
        sig = aux.localExternSigs[as][dotMember];
    }
    // Determine the funId and sig otherwise
    else if (!as.size()) {
        if (!aux.globalExterns.count(dotMember))
            err("Invalid call: "+printSimple(dotOwner)+"."+dotMember, m);
        std::string key = unsignedToDecimal(aux.globalExterns[dotMember]);
        funId = aux.globalExterns[dotMember];
        sig = aux.globalExternSigs[dotMember];
    }
    else err("Invalid call: "+printSimple(dotOwner)+"."+dotMember, m);
    // Pack arguments
    kwargs["data"] = packArguments(fnargs, sig, funId, m);
    kwargs["to"] = dotOwner;
    Node main;
    // Pack output
    if (!kwargs.count("outsz")) {
        main = parseLLL(
            "(with _data $data (seq "
                "(pop (~"+op+" $gas $to $value (access _data 0) (access _data 1) (ref $dataout) 32))"
                "(get $dataout)))");
    }
    else {
        main = parseLLL(
            "(with _data $data (with _outsz $outsz (with _out (array _outsz) (seq "
                "(pop (~"+op+" $gas $to $value (access _data 0) (access _data 1) _out (mul 32 _outsz)))"
                "(get _out)))))");
    }
    // Set up main call

    Node o = subst(main, kwargs, prefix, m);
    return o;
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
        std::string t = "_temp_"+mkUniqueToken();
        std::vector<Node> sub;
        for (unsigned i = 0; i < terms.size(); i++)
            sub.push_back(asn("mstore",
                              asn("add",
                                  tkn(utd(i * 32), m),
                                  asn("get", tkn(t+"pos", m), m),
                                  m),
                              terms[i],
                              m));
        sub.push_back(tkn(t+"pos", m));
        Node main = asn("with",
                        tkn(t+"pos", m),
                        asn("alloc", tkn(utd(terms.size() * 32), m), m),
                        asn("seq", sub, m),
                        m);
        Node sz = token(utd(terms.size() * 32), m);
        o = astnode("~sha3",
                    main,
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

// Basic rewrite rule execution
std::pair<Node, bool> rulesTransform(Node node, rewriteRuleSet macros) {
    std::string prefix = "_temp_"+mkUniqueToken();
    bool changed = false;
    if (!macros.ruleLists.count(node.val))
        return std::pair<Node, bool>(node, false);
    std::vector<rewriteRule> rules = macros.ruleLists[node.val];
    for (unsigned pos = 0; pos < rules.size(); pos++) {
        rewriteRule macro = rules[pos];
        matchResult mr = match(macro.pattern, node);
        if (mr.success) {
            node = subst(macro.substitution, mr.map, prefix, node.metadata);
            std::pair<Node, bool> o = rulesTransform(node, macros);
            o.second = true;
            return o;
        }
    }
    return std::pair<Node, bool>(node, changed);
}

std::pair<Node, bool> synonymTransform(Node node) {
    bool changed = false;
    if (node.type == ASTNODE && synonymMap.count(node.val)) {
        node.val = synonymMap[node.val];
        changed = true;
    }
    return std::pair<Node, bool>(node, changed);
}

rewriteRuleSet nodeMacros;
rewriteRuleSet setterMacros;

bool dontDescend(std::string s) {
    return s == "macro" || s == "comment" || s == "outer";
}

// Recursively applies any set of rewrite rules
std::pair<Node, bool> apply_rules_iter(preprocessResult pr, rewriteRuleSet rules) {
    bool changed = false;
    Node node = pr.first;
    if (dontDescend(node.val))
        return std::pair<Node, bool>(node, false);
    std::pair<Node, bool> o = rulesTransform(node, rules);
    node = o.first;
    changed = changed || o.second;
    if (node.type == ASTNODE) {
        for (unsigned i = 0; i < node.args.size(); i++) {
            std::pair<Node, bool> r =
                apply_rules_iter(preprocessResult(node.args[i], pr.second), rules);
            node.args[i] = r.first;
            changed = changed || r.second;
        }
    }
    return std::pair<Node, bool>(node, changed);
}

// Recursively applies rewrite rules and other primary transformations
std::pair<Node, bool> mainTransform(preprocessResult pr) {
    bool changed = false;
    Node node = pr.first;

    // Anything inside "outer" should be treated as a separate program
    // and thus recursively compiled in its entirety
    if (node.val == "outer") {
        node = apply_rules(preprocess(node.args[0]));
        changed = true;
    }

    // Don't descend into comments, macros and inner scopes
    if (dontDescend(node.val))
        return std::pair<Node, bool>(node, changed);

    // Special storage transformation
    if (isNodeStorageVariable(node)) {
        node = storageTransform(node, pr.second);
        changed = true;
    }
    if (node.val == "ref" && isNodeStorageVariable(node.args[0])) {
        node = storageTransform(node.args[0], pr.second, false, true);
        changed = true;
    }
    if (node.val == "=" && isNodeStorageVariable(node.args[0])) {
        Node t = storageTransform(node.args[0], pr.second);
        if (t.val == "sload") {
            std::vector<Node> o;
            o.push_back(t.args[0]);
            o.push_back(node.args[1]);
            node = astnode("sstore", o, node.metadata);
        }
        changed = true;
    }
    // Main code
    std::pair<Node, bool> pnb = synonymTransform(node);
    node = pnb.first;
    changed = changed || pnb.second;
    // std::cerr << priority << " " << macros.size() << "\n";
    std::pair<Node, bool> pnc = rulesTransform(node, nodeMacros);
    node = pnc.first;
    changed = changed || pnc.second;


    // Special transformations
    if (node.val == "log") {
        node = log_transform(node);
        changed = true;
    }
    if (node.val == "array_lit") {
        node = array_lit_transform(node);
        changed = true;
    }
    if (node.val == "fun" && node.args[0].val == ".") {
        node = dotTransform(node, pr.second);
        changed = true;
    }
    if (node.val == "text") {
        node = string_transform(node);
        changed = true;
    }
    if (node.type == ASTNODE) {
		unsigned i = 0;
        // Arg 0 of all of these is a variable, so should not be changed
        if (node.val == "set" || node.val == "ref" 
                || node.val == "get" || node.val == "with") {
            if (node.args[0].type == TOKEN && 
                    node.args[0].val.size() > 0 && node.args[0].val[0] != '\'') {
                node.args[0].val = "'" + node.args[0].val;
                changed = true;
            }
            i = 1;
        }
        // Recursively process children
        for (; i < node.args.size(); i++) {
            std::pair<Node, bool> r =
                mainTransform(preprocessResult(node.args[i], pr.second));
            node.args[i] = r.first;
            changed = changed || r.second;
        }
    }
    // Add leading ' to variable names, and wrap them inside get
    else if (node.type == TOKEN && !isNumberLike(node)) {
        if (node.val.size() && node.val[0] != '\'' && node.val[0] != '$') {
            Node n = astnode("get", tkn("'"+node.val), node.metadata);
            node = n;
            changed = true;
        }
    }
    // Convert all numbers to normalized form
    else if (node.type == TOKEN && isNumberLike(node) && !isDecimal(node.val)) {
        node.val = strToNumeric(node.val);
        changed = true;
    }
    return std::pair<Node, bool>(node, changed);
}

// Do some preprocessing to convert all of our macro lists into compiled
// forms that can then be reused
void parseMacros() {
    for (int i = 0; i < 9999; i++) {
        std::vector<Node> o;
        if (macros[i][0] == "---END---") break;
        nodeMacros.addRule(rewriteRule(
            parseLLL(macros[i][0]),
            parseLLL(macros[i][1])
        ));
    }
    for (int i = 0; i < 9999; i++) {
        std::vector<Node> o;
        if (setters[i][0] == "---END---") break;
        setterMacros.addRule(rewriteRule(
            asn(setters[i][0], tkn("$x"), tkn("$y")),
            asn("=", tkn("$x"), asn(setters[i][1], tkn("$x"), tkn("$y")))
        ));
    }
    for (int i = 0; i < 9999; i++) {
        if (synonyms[i][0] == "---END---") break;
        synonymMap[synonyms[i][0]] = synonyms[i][1];
    }
}

Node apply_rules(preprocessResult pr) {
    // If the rewrite rules have not yet been parsed, parse them
    if (!nodeMacros.ruleLists.size()) parseMacros();
    // Iterate over macros by priority list
    std::map<int, rewriteRuleSet >::iterator it;
    std::pair<Node, bool> r;
    for(it=pr.second.customMacros.begin();
        it != pr.second.customMacros.end(); it++) {
        while (1) {
            // std::cerr << "STARTING ARI CYCLE: " << (*it).first <<"\n";
            // std::cerr << printAST(pr.first) << "\n";
            r = apply_rules_iter(pr, (*it).second);
            pr.first = r.first;
            if (!r.second) break;
        }
    }
    // Apply setter macros
    while (1) {
        r = apply_rules_iter(pr, setterMacros);
        pr.first = r.first;
        if (!r.second) break;
    }
    // Apply all other mactos
    while (1) {
        r = mainTransform(pr);
        pr.first = r.first;
        if (!r.second) break;
    }
    return r.first;
}

// Pre-validation
Node validate(Node inp) {
    Metadata m = inp.metadata;
    if (inp.type == ASTNODE) {
        int i = 0;
        while(validFunctions[i][0] != "---END---") {
            if (inp.val == validFunctions[i][0]) {
                std::string sz = unsignedToDecimal(inp.args.size());
                if (decimalGt(validFunctions[i][1], sz)) {
                    err("Too few arguments for "+inp.val, inp.metadata);   
                }
                if (decimalGt(sz, validFunctions[i][2])) {
                    err("Too many arguments for "+inp.val, inp.metadata);   
                }
            }
            i++;
        }
    }
    else if (inp.type == TOKEN) {
        if (!inp.val.size()) err("??? empty token", m);
        if (inp.val[0] == '_') err("Variables cannot start with _", m);
    }
	for (unsigned i = 0; i < inp.args.size(); i++) validate(inp.args[i]);
    return inp;
}

Node postValidate(Node inp) {
    // This allows people to use ~x as a way of having functions with the same
    // name and arity as macros; the idea is that ~x is a "final" form, and 
    // should not be remacroed, but it is converted back at the end
    if (inp.val.size() > 0 && inp.val[0] == '~') {
        inp.val = inp.val.substr(1);
    }
    if (inp.type == ASTNODE) {
        if (inp.val == ".")
            err("Invalid object member (ie. a foo.bar not mapped to anything)",
                inp.metadata);
        else if (opcode(inp.val) >= 0) {
            if ((signed)inp.args.size() < opinputs(inp.val))
                err("Too few arguments for "+inp.val, inp.metadata);
            if ((signed)inp.args.size() > opinputs(inp.val))
                err("Too many arguments for "+inp.val, inp.metadata);
        }
        else if (isValidLLLFunc(inp.val, inp.args.size())) {
            // do nothing
        }
        else err ("Invalid argument count or LLL function: "+printSimple(inp), inp.metadata);
        for (unsigned i = 0; i < inp.args.size(); i++) {
            inp.args[i] = postValidate(inp.args[i]);
        }
    }
    return inp;
}


Node rewriteChunk(Node inp) {
    return postValidate(optimize(apply_rules(
                        preprocessResult(
                        validate(inp), preprocessAux()))));
}

// Flatten nested sequence into flat sequence
Node flattenSeq(Node inp) {
    std::vector<Node> o;
    if (inp.val == "seq" && inp.type == ASTNODE) {
        for (unsigned i = 0; i < inp.args.size(); i++) {
            if (inp.args[i].val == "seq" && inp.args[i].type == ASTNODE)
                o = extend(o, flattenSeq(inp.args[i]).args);
            else
                o.push_back(flattenSeq(inp.args[i]));
        }
    }
    else if (inp.type == ASTNODE) {
        for (unsigned i = 0; i < inp.args.size(); i++) {
            o.push_back(flattenSeq(inp.args[i]));
        }
    }
    else return inp;
    return asn(inp.val, o, inp.metadata);
}

Node rewrite(Node inp) {
    return postValidate(optimize(apply_rules(preprocess(flattenSeq(inp)))));
}

using namespace std;
