#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"
#include "lllparser.h"
#include "bignum.h"

std::string valid[][3] = {
    { "if", "2", "3" },
    { "unless", "2", "2" },
    { "while", "2", "2" },
    { "until", "2", "2" },
    { "code", "1", "2" },
    { "init", "2", "2" },
    { "shared", "2", "3" },
    { "alloc", "1", "1" },
    { "array", "1", "1" },
    { "call", "2", tt256 },
    { "call_code", "2", tt256 },
    { "create", "1", "4" },
    { "msg", "4", "6" },
    { "msg_stateless", "4", "6" },
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
        "(@/= $a $b)",
        "(set $a (@/ $a $b))"
    },
    {
        "(@%= $a $b)",
        "(set $a (@% $a $b))"
    },
    {
        "(!= $a $b)",
        "(iszero (eq $a $b))"
    },
    {
        "(min a b)",
        "(with $1 a (with $2 b (if (lt $1 $2) $1 $2)))"
    },
    {
        "(max a b)",
        "(with $1 a (with $2 b (if (lt $1 $2) $2 $1)))"
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
        "(access msg.data $ind)",
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
        "(access contract.storage $ind)",
        "(sload $ind)"
    },
    {
        "(access $var $ind)",
        "(mload (add $var (mul 32 $ind)))"
    },
    {
        "(set (access contract.storage $ind) $val)",
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
        "(return $start $len)",
        "(~return $start (mul 32 $len))"
    },
    {
        "(&& $x $y)",
        "(if $x $y 0)"
    },
    {
        "(|| $x $y)",
        "(with $1 $x (if (get $1) (get $1) $y))"
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
    // Wrappers
    {
        "(outer (init $init $code))",
        "(seq $init (~return 0 (lll $code 0)))"
    },
    {
        "(outer (shared $shared (init $init (code $code))))",
        "(seq $shared $init (~return 0 (lll (seq $shared $code) 0)))"
    },
    {
        "(outer $code)",
        "(~return 0 (lll $code 0))"
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
        "(~log1 $t1 0 0)"
    },
    {
        "(log $t1 $t2)",
        "(~log2 $t1 $t2 0 0)"
    },
    {
        "(log $t1 $t2 $t3)",
        "(~log3 $t1 $t2 $t3 0 0)"
    },
    {
        "(log $t1 $t2 $t3 $t4)",
        "(~log4 $t1 $t2 $t3 $t4 0 0)"
    },
    { "msg.datasize", "(div (calldatasize) 32)" },
    { "msg.sender", "(caller)" },
    { "msg.value", "(callvalue)" },
    { "tx.gasprice", "(gasprice)" },
    { "tx.origin", "(origin)" },
    { "tx.gas", "(gas)" },
    { "contract.balance", "(balance (address))" },
    { "contract.address", "(address)" },
    { "block.prevhash", "(prevhash)" },
    { "block.coinbase", "(coinbase)" },
    { "block.timestamp", "(timestamp)" },
    { "block.number", "(number)" },
    { "block.difficulty", "(difficulty)" },
    { "block.gaslimit", "(gaslimit)" },
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

struct matchResult {
    bool success;
    std::map<std::string, Node> map;
};

// Returns two values. First, a boolean to determine whether the node matches
// the pattern, second, if the node does match then a map mapping variables
// in the pattern to nodes
matchResult match(Node p, Node n) {
    matchResult o;
    o.success = false;
    if (p.type == TOKEN) {
        if (p.val == n.val && n.type == TOKEN) o.success = true;
        else if (p.val[0] == '$') {
            o.success = true;
            o.map[p.val.substr(1)] = n;
        }
    }
    else if (n.type==TOKEN || p.val!=n.val || p.args.size()!=n.args.size()) {
    }
    else {
		for (unsigned i = 0; i < p.args.size(); i++) {
            matchResult oPrime = match(p.args[i], n.args[i]);
            if (!oPrime.success) {
                o.success = false;
                return o;
            }
            for (std::map<std::string, Node>::iterator it = oPrime.map.begin();
                 it != oPrime.map.end();
                 it++) {
                o.map[(*it).first] = (*it).second;
            }
        }
        o.success = true;
    }
    return o;
}

// Fills in the pattern with a dictionary mapping variable names to
// nodes (these dicts are generated by match). Match and subst together
// create a full pattern-matching engine. 
Node subst(Node pattern,
           std::map<std::string, Node> dict,
           std::string varflag,
           Metadata metadata) {
    if (pattern.type == TOKEN && pattern.val[0] == '$') {
        if (dict.count(pattern.val.substr(1))) {
            return dict[pattern.val.substr(1)];
        }
        else {
            return token(varflag + pattern.val.substr(1), metadata);
        }
    }
    else if (pattern.type == TOKEN) {
        return pattern;
    }
    else {
        std::vector<Node> args;
		for (unsigned i = 0; i < pattern.args.size(); i++) {
            args.push_back(subst(pattern.args[i], dict, varflag, metadata));
        }
        return astnode(pattern.val, args, metadata);
    }
}

// array_lit transform

Node array_lit_transform(Node node) {
    std::vector<Node> o1;
    o1.push_back(token(unsignedToDecimal(node.args.size() * 32), node.metadata));
    std::vector<Node> o2;
    std::string symb = "_temp"+mkUniqueToken()+"_0";
    o2.push_back(token(symb, node.metadata));
    o2.push_back(astnode("alloc", o1, node.metadata));
    std::vector<Node> o3;
    o3.push_back(astnode("set", o2, node.metadata));
    for (unsigned i = 0; i < node.args.size(); i++) {
        // (mstore (add (get symb) i*32) v)
        std::vector<Node> o5;
        o5.push_back(token(symb, node.metadata));
        std::vector<Node> o6;
        o6.push_back(astnode("get", o5, node.metadata));
        o6.push_back(token(unsignedToDecimal(i * 32), node.metadata));
        std::vector<Node> o7;
        o7.push_back(astnode("add", o6));
        o7.push_back(node.args[i]);
        o3.push_back(astnode("mstore", o7, node.metadata));
    }
    std::vector<Node> o8;
    o8.push_back(token(symb, node.metadata));
    o3.push_back(astnode("get", o8));
    return astnode("seq", o3, node.metadata);
}

// call transform

#define psn std::pair<std::string, Node>

Node call_transform(Node node, std::string op) {
    std::string prefix = "_temp"+mkUniqueToken()+"_";
    std::map<std::string, Node> kwargs;
    kwargs["value"] = token("0", node.metadata);
    kwargs["gas"] = parseLLL("(- (gas) 25)");
    std::vector<Node> args;
    for (unsigned i = 0; i < node.args.size(); i++) {
        if (node.args[i].val == "=" || node.args[i].val == "set") {
            kwargs[node.args[i].args[0].val] = node.args[i].args[1];
        }
        else args.push_back(node.args[i]);
    }
    if (args.size() < 2) err("Too few arguments for call!", node.metadata);
    kwargs["to"] = args[0];
    kwargs["funid"] = args[1];
    std::vector<Node> inputs;
    for (unsigned i = 2; i < args.size(); i++) {
        inputs.push_back(args[i]);
    }
    std::vector<psn> with;
    std::vector<Node> precompute;
    std::vector<Node> post;
    if (kwargs.count("data")) {
        if (!kwargs.count("datasz")) {
            err("Required param datasz", node.metadata);
        }
        // store data: data array start
        with.push_back(psn(prefix+"data", kwargs["data"]));
        // store data: prior: data array - 32
        std::vector<Node> argz;
        argz.push_back(token(prefix+"data", node.metadata));
        argz.push_back(token("32", node.metadata));
        with.push_back(psn(prefix+"prior", astnode("sub", argz, node.metadata)));
        // store data: priormem: data array - 32 prior memory value
        std::vector<Node> argz2;
        argz2.push_back(token(prefix+"prior", node.metadata));
        with.push_back(psn(prefix+"priormem", astnode("mload", argz2, node.metadata)));
        // post: reinstate prior mem at data array - 32
        std::vector<Node> argz3;
        argz3.push_back(token(prefix+"prior", node.metadata));
        argz3.push_back(token(prefix+"priormem", node.metadata));
        post.push_back(astnode("mstore", argz3, node.metadata));
        // store data: datastart: data array - 1
        std::vector<Node> argz4;
        argz4.push_back(token(prefix+"data", node.metadata));
        argz4.push_back(token("1", node.metadata));
        with.push_back(psn(prefix+"datastart", astnode("sub", argz4, node.metadata)));
        // push funid byte to datastart
        std::vector<Node> argz5;
        argz5.push_back(token(prefix+"datastart", node.metadata));
        argz5.push_back(kwargs["funid"]);
        precompute.push_back(astnode("mstore8", argz5, node.metadata));
        // set data array start loc
        kwargs["datain"] = token(prefix+"datastart", node.metadata);
        std::vector<Node> argz6;
        argz6.push_back(token("32", node.metadata));
        argz6.push_back(kwargs["datasz"]);
        std::vector<Node> argz7;
        argz7.push_back(astnode("mul", argz6, node.metadata));
        argz7.push_back(token("1", node.metadata));
        kwargs["datainsz"] = astnode("add", argz7, node.metadata);
    }
    else {
        // Pre-declare variables; relies on declared variables being sequential
        std::vector<Node> argz;
        argz.push_back(token(prefix+"prebyte", node.metadata));
        precompute.push_back(astnode("declare", argz, node.metadata));
        for (unsigned i = 0; i < inputs.size(); i++) {
            std::vector<Node> argz;
            argz.push_back(token(prefix+unsignedToDecimal(i), node.metadata));
            precompute.push_back(astnode("declare", argz, node.metadata));
        }
        // Set up variables
        std::vector<Node> argz1;
        argz1.push_back(token(prefix+"prebyte", node.metadata));
        std::vector<Node> argz2;
        argz2.push_back(astnode("ref", argz1, node.metadata));
        argz2.push_back(token("31", node.metadata));
        std::vector<Node> argz3;
        argz3.push_back(astnode("add", argz2, node.metadata));
        argz3.push_back(kwargs["funid"]);
        precompute.push_back(astnode("mstore8", argz3, node.metadata));
        for (unsigned i = 0; i < inputs.size(); i++) {
            std::vector<Node> argz;
            argz.push_back(token(prefix+unsignedToDecimal(i), node.metadata));
            argz.push_back(inputs[i]);
            precompute.push_back(astnode("set", argz, node.metadata));
        }
        kwargs["datain"] = astnode("add", argz2, node.metadata);
        kwargs["datainsz"] = token(unsignedToDecimal(inputs.size() * 32 + 1), node.metadata);
    }
    if (!kwargs.count("outsz")) {
        std::vector<Node> argz11;
        argz11.push_back(token(prefix+"dataout", node.metadata));
        precompute.push_back(astnode("declare", argz11, node.metadata));
        kwargs["dataout"] = astnode("ref", argz11, node.metadata);
        kwargs["dataoutsz"] = token("32", node.metadata);
        std::vector<Node> argz12;
        argz12.push_back(token(prefix+"dataout", node.metadata));
        post.push_back(astnode("get", argz12, node.metadata));
    }
    else {
        kwargs["dataout"] = kwargs["out"];
        kwargs["dataoutsz"] = kwargs["outsz"];
        std::vector<Node> argz12;
        argz12.push_back(token(prefix+"dataout", node.metadata));
        post.push_back(astnode("ref", argz12, node.metadata));
    }
        
    std::vector<Node> main;
    for (unsigned i = 0; i < precompute.size(); i++) {
        main.push_back(precompute[i]);
    }
    std::vector<Node> call;
    call.push_back(kwargs["gas"]);
    call.push_back(kwargs["to"]);
    call.push_back(kwargs["value"]);
    call.push_back(kwargs["datain"]);
    call.push_back(kwargs["datainsz"]);
    call.push_back(kwargs["dataout"]);
    call.push_back(kwargs["dataoutsz"]);
    std::vector<Node> argz20;
    argz20.push_back(astnode("~"+op, call, node.metadata));
    main.push_back(astnode("pop", argz20, node.metadata));
    for (unsigned i = 0; i < post.size(); i++) {
        main.push_back(post[i]);
    }
    Node mainNode = astnode("seq", main, node.metadata);
    for (int i = with.size() - 1; i >= 0; i--) {
        std::string varname = with[i].first;
        Node varnode = with[i].second;
        std::vector<Node> argz30;
        argz30.push_back(token(varname, node.metadata));
        argz30.push_back(varnode);
        argz30.push_back(mainNode);
        mainNode = astnode("with", argz30, node.metadata);
    }
    return mainNode;
}

// Recursively applies rewrite rules
Node apply_rules(Node node) {
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
        if (mr.success && node.val != "call" && node.val != "call_code") {
            Node pattern2 = nodeMacros[pos][1];
            node = subst(pattern2, mr.map, prefix, node.metadata);
            pos = 0;
        }
    }
    // Array_lit special instruction
    if (node.val == "array_lit")
        node = array_lit_transform(node);
    if (node.val == "call")
        node = call_transform(node, "call");
    if (node.val == "call_code")
        node = call_transform(node, "call_code");
    if (node.type == ASTNODE) {
		unsigned i = 0;
        if (node.val == "set" || node.val == "ref" 
                || node.val == "get" || node.val == "with"
                || node.val == "def") {
            node.args[0].val = "'" + node.args[0].val;
            i = 1;
        }
        if (node.val == "def") {
            for (unsigned j = 0; j < node.args[0].args.size(); j++) {
                if (node.args[0].args[j].val == ":") {
                    node.args[0].args[j].val = "kv";
                    node.args[0].args[j].args[0].val =
                         "'" + node.args[0].args[j].args[0].val;
                }
                else {
                    node.args[0].args[j].val = "'" + node.args[0].args[j].val;
                }
            }
        }
        for (; i < node.args.size(); i++) {
            node.args[i] = apply_rules(node.args[i]);
        }
    }
    else if (node.type == TOKEN && !isNumberLike(node)) {
        node.val = "'" + node.val;
        std::vector<Node> args;
        args.push_back(node);
        node = astnode("get", args, node.metadata);
    }
    // This allows people to use ~x as a way of having functions with the same
    // name and arity as macros; the idea is that ~x is a "final" form, and 
    // should not be remacroed, but it is converted back at the end
    if (node.type == ASTNODE && node.val[0] == '~')
        node.val = node.val.substr(1);
    return node;
}

Node optimize(Node inp) {
    if (inp.type == TOKEN) {
        Node o = tryNumberize(inp);
        if (decimalGt(o.val, tt256, true))
            err("Value too large (exceeds 32 bytes or 2^256)", inp.metadata);
        return o;
    }
	for (unsigned i = 0; i < inp.args.size(); i++) {
        inp.args[i] = optimize(inp.args[i]);
    }
    if (inp.args.size() == 2 
            && inp.args[0].type == TOKEN 
            && inp.args[1].type == TOKEN) {
      std::string o;
      if (inp.val == "add") {
          o = decimalMod(decimalAdd(inp.args[0].val, inp.args[1].val), tt256);
      }
      else if (inp.val == "sub") {
          if (decimalGt(inp.args[0].val, inp.args[1].val, true))
              o = decimalSub(inp.args[0].val, inp.args[1].val);
      }
      else if (inp.val == "mul") {
          o = decimalMod(decimalMul(inp.args[0].val, inp.args[1].val), tt256);
      }
      else if (inp.val == "div" && inp.args[1].val != "0") {
          o = decimalDiv(inp.args[0].val, inp.args[1].val);
      }
      else if (inp.val == "sdiv" && inp.args[1].val != "0"
            && decimalGt(tt255, inp.args[0].val)
            && decimalGt(tt255, inp.args[1].val)) {
          o = decimalDiv(inp.args[0].val, inp.args[1].val);
      }
      else if (inp.val == "mod" && inp.args[1].val != "0") {
          o = decimalMod(inp.args[0].val, inp.args[1].val);
      }
      else if (inp.val == "smod" && inp.args[1].val != "0"
            && decimalGt(tt255, inp.args[0].val)
            && decimalGt(tt255, inp.args[1].val)) {
          o = decimalMod(inp.args[0].val, inp.args[1].val);
      }    
      if (o.length()) return token(o, inp.metadata);
    }
    return inp;
}

Node validate(Node inp) {
    if (inp.type == ASTNODE) {
        int i = 0;
        while(valid[i][0] != "---END---") {
            if (inp.val == valid[i][0]) {
                if (decimalGt(valid[i][1], unsignedToDecimal(inp.args.size()))) {
                    err("Too few arguments for "+inp.val, inp.metadata);   
                }
                if (decimalGt(unsignedToDecimal(inp.args.size()), valid[i][2])) {
                    err("Too many arguments for "+inp.val, inp.metadata);   
                }
            }
            i++;
        }
    }
	for (unsigned i = 0; i < inp.args.size(); i++) validate(inp.args[i]);
    return inp;
}

Node preprocess(Node inp) {
    std::vector<Node> args;
    if (inp.val == "seq") {
        for (unsigned i = 0; i < inp.args.size(); i++) {
            if (inp.args[i].val == "def") {
                if (inp.args[i].args.size() == 0)
                    err("Empty def", inp.metadata);
                if (inp.args[i].args[0].val == "init") {
                    if (inp.args[i].args[0].args.size())
                        err("Init cannot have arguments", inp.metadata);
                    std::vector<Node> new_outer;   
                    new_outer.push_back(inp.args[i].args[1]);
                    std::vector<Node> new_inner;
                    for (unsigned j = 0; j < inp.args.size(); j++) {
                        if (j != i) new_inner.push_back(inp.args[j]);
                    }
                    new_outer.push_back(astnode("seq", new_inner, inp.metadata));
                    args.push_back(astnode("init", new_outer, inp.metadata));
                    return astnode("outer", args, inp.metadata);
                }
            }
        }
    }
    args.push_back(inp);
    return astnode("outer", args, inp.metadata);
}

Node rewrite(Node inp) {
    return optimize(apply_rules(validate(preprocess(inp))));
}

Node rewriteChunk(Node inp) {
    return optimize(apply_rules(validate(inp)));
}

using namespace std;
