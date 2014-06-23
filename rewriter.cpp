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
    { "call", "2", "4" },
    { "create", "1", "4" },
    { "msg", "4", "6" },
    { "getch", "2", "2" },
    { "setch", "3", "3" },
    { "sha3", "1", "2" },
    { "return", "1", "2" },
    { "inset", "1", "1" },
    { "import", "1", "1" },
    { "array_lit", "0", tt256 },
    { "seq", "0", tt256 },
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
        "(if $cond $do (else $else))",
        "(if $cond $do $else)"
    },
    {
        "(code $code)",
        "$code"
    },
    {
        "(access msg.data $ind)",
        "(CALLDATALOAD (MUL 32 $ind))"
    },
    {
        "(array $len)",
        "(alloc (MUL 32 $len))"
    },
    {
        "(while $cond $do)",
        "(until (NOT $cond) $do)",
    },
    {
        "(while (NOT $cond) $do)",
        "(until $cond $do)",
    },
    {
        "(if $cond $do)",
        "(unless (NOT $cond) $do)",
    },
    {
        "(if (NOT $cond) $do)",
        "(unless $cond $do)",
    },
    {
        "(access contract.storage $ind)",
        "(SLOAD $ind)"
    },
    {
        "(access $var $ind)",
        "(MLOAD (ADD $var (MUL 32 $ind)))"
    },
    {
        "(set (access contract.storage $ind) $val)",
        "(SSTORE $ind $val)"
    },
    {
        "(set (access $var $ind) $val)",
        "(MSTORE (ADD $var (MUL 32 $ind)) $val)"
    },
    {
        "(getch $var $ind)",
        "(MOD (MLOAD (ADD $var $ind)) 256)"
    },
    {
        "(setch $var $ind $val)",
        "(MSTORE8 (ADD $var $ind) $val)",
    },
    {
        "(send $to $value)",
        "(CALL (SUB (GAS) 25) $to $value 0 0 0 0)"
    },
    {
        "(send $gas $to $value)",
        "(CALL $gas $to $value 0 0 0 0)"
    },
    {
        "(sha3 $x)",
        "(seq (set $1 $x) (SHA3 (ref $1) 32))"
    },
    {
        "(sha3 $start $len)",
        "(SHA3 $start $len)"
    },
    {
        "(calldataload $start $len)",
        "(CALLDATALOAD $start (MUL 32 $len))"
    },
    {
        "(id $0)",
        "$0"
    },
    {
        "(return $x)",
        "(seq (set $1 $x) (RETURN (ref $1) 32))"
    },
    {
        "(return $start $len)",
        "(RETURN $start (MUL 32 $len))"
    },
    {
        "(&& $x $y)",
        "(if $x $y 0)"
    },
    {
        "(|| $x $y)",
        "(seq (set $1 $x) (if (get $1) (get $1) $y))"
    },
    {
        "(>= $x $y)",
        "(NOT (SLT $x $y))"
    },
    {
        "(<= $x $y)",
        "(NOT (SGT $x $y))"
    },
    {
        "(@>= $x $y)",
        "(NOT (LT $x $y))"
    },
    {
        "(@<= $x $y)",
        "(NOT (GT $x $y))"
    },
    {
        "(create $code)",
        "(create 0 $code)"
    },
    {
        "(create $endowment $code)",
        "(seq (set $1 (MSIZE)) (CREATE $endowment (ref $1) (lll (outer $code) (MSIZE))))"
    },
    {
        "(msg $gas $to $val $dataval)",
        "(seq (set $1 $dataval) (CALL $gas $to $val (ref $1) 32 (ref $2) 32) (get $2))"
    },
    {
        "(call $f $dataval)",
        "(msg (SUB (GAS) 45) $f 0 $dataval)"
    },
    {
        "(msg $gas $to $val $inp $inpsz)",
        "(seq (CALL $gas $to $val $inp (MUL 32 $inpsz) (ref $1) 32) (get $1))"
    },
    {
        "(call $f $inp $inpsz)",
        "(msg (SUB (GAS) 25) $f 0 $inp $inpsz)"
    },
    {
        "(call $f $inp $inpsz $outsz)",
        "(seq (set $1 $outsz) (set $2 (alloc (MUL 32 (get $1)))) (POP (CALL (SUB (GAS) (ADD 25 (get $1))) $f 0 $inp (MUL 32 $inpsz) (ref $2) (MUL 32 (get $1)))) (get $2))"
    },
    {
        "(msg $gas $to $val $inp $inpsz $outsz)",
        "(seq (set $1 (MUL 32 $outsz)) (set $2 (alloc (get $1))) (POP (CALL $gas $to $val $inp (MUL 32 $inpsz) (ref $2) (get $1))) (get $2))"
    },
    {
        "(outer (init $init $code))",
        "(seq $init (RETURN 0 (lll $code 0)))"
    },
    {
        "(outer (shared $shared (init $init (code $code))))",
        "(seq $shared $init (RETURN 0 (lll (seq $shared $code) 0)))"
    },
    {
        "(outer $code)",
        "(RETURN 0 (lll $code 0))"
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
        "(create $val (import $code))",
        "(seq (set $1 MSIZE) (create $val (get $1) (lll $code (get $1))))"
    },
    {
        "(create (import $x))",
        "(seq (set $1 MSIZE) (create $val (get $1) (lll $code (get $1))))"
    },
    {
        "(create $x)",
        "(seq (set $1 MSIZE) (create $val (get $1) (lll $code (get $1))))"
    },
    { "msg.datasize", "(DIV (CALLDATASIZE) 32)" },
    { "msg.sender", "(CALLER)" },
    { "msg.value", "(CALLVALUE)" },
    { "tx.gasprice", "(GASPRICE)" },
    { "tx.origin", "(ORIGIN)" },
    { "tx.gas", "(GAS)" },
    { "contract.balance", "(BALANCE)" },
    { "contract.address", "(ADDRESS)" },
    { "block.prevhash", "(PREVHASH)" },
    { "block.coinbase", "(COINBASE)" },
    { "block.timestamp", "(TIMESTAMP)" },
    { "block.number", "(NUMBER)" },
    { "block.difficulty", "(DIFFICULTY)" },
    { "block.gaslimit", "(GASLIMIT)" },
    { "stop", "(STOP)" },
    { "---END---", "" } //Keep this line at the end of the list
};

std::vector<std::vector<Node> > nodeMacros;

std::string synonyms[][2] = {
    { "|", "OR" },
    { "or", "||" },
    { "&", "AND" },
    { "and", "&&" },
    { "xor", "XOR" },
    { "elif", "if" },
    { "!", "NOT" },
    { "not", "NOT" },
    { "not", "NOT" },
    { "byte", "BYTE" },
    { "string", "alloc" },
    { "+", "ADD" },
    { "-", "SUB" },
    { "*", "MUL" },
    { "/", "SDIV" },
    { "^", "EXP" },
    { "%", "SMOD" },
    { "@/", "DIV" },
    { "@%", "MOD" },
    { "@<", "LT" },
    { "@>", "GT" },
    { "<", "SLT" },
    { ">", "SGT" },
    { "=", "set" },
    { "==", "EQ" },
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
        if (p.val == n.val) o.success = true;
        else if (p.val[0] == '$') {
            o.success = true;
            o.map[p.val.substr(1)] = n;
        }
    }
    else if (n.type==TOKEN || p.val!=n.val || p.args.size()!=n.args.size()) {
    }
    else {
        for (int i = 0; i < p.args.size(); i++) {
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
        for (int i = 0; i < pattern.args.size(); i++) {
            args.push_back(subst(pattern.args[i], dict, varflag, metadata));
        }
        return astnode(pattern.val, args, metadata);
    }
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
    int pos = 0;
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
        }
    }
    if (node.type == ASTNODE && node.val != "ref" && node.val != "get") {
        int i = 0;
        if (node.val == "set") i = 1;
        for (i = i; i < node.args.size(); i++) {
            node.args[i] = apply_rules(node.args[i]);
        }
    }
    else if (node.type == TOKEN && !isNumberLike(node)) {
        std::vector<Node> args;
        args.push_back(node);
        node = astnode("get", args, node.metadata);
    }
    return node;
}

Node optimize(Node inp) {
    if (inp.type == TOKEN) return tryNumberize(inp);
    for (int i = 0; i < inp.args.size(); i++) {
        inp.args[i] = optimize(inp.args[i]);
    }
    if (inp.args.size() == 2 
            && inp.args[0].type == TOKEN 
            && inp.args[1].type == TOKEN) {
      std::string o;
      if (inp.val == "ADD") {
          o = decimalMod(decimalAdd(inp.args[0].val, inp.args[1].val), tt256);
      }
      else if (inp.val == "SUB") {
          if (decimalGt(inp.args[0].val, inp.args[1].val, true))
              o = decimalSub(inp.args[0].val, inp.args[1].val);
      }
      else if (inp.val == "MUL") {
          o = decimalMod(decimalMul(inp.args[0].val, inp.args[1].val), tt256);
      }
      else if (inp.val == "DIV" && inp.args[1].val != "0") {
          o = decimalDiv(inp.args[0].val, inp.args[1].val);
      }
      else if (inp.val == "SDIV" && inp.args[1].val != "0"
            && decimalGt(tt255, inp.args[0].val)
            && decimalGt(tt255, inp.args[1].val)) {
          o = decimalDiv(inp.args[0].val, inp.args[1].val);
      }
      else if (inp.val == "MOD" && inp.args[1].val != "0") {
          o = decimalMod(inp.args[0].val, inp.args[1].val);
      }
      else if (inp.val == "SMOD" && inp.args[1].val != "0"
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
                if (decimalGt(valid[i][1], intToDecimal(inp.args.size()))) {
                    err("Too few arguments for "+inp.val, inp.metadata);   
                }
                if (decimalGt(intToDecimal(inp.args.size()), valid[i][2])) {
                    err("Too many arguments for "+inp.val, inp.metadata);   
                }
            }
            i++;
        }
    }
    for (int i = 0; i < inp.args.size(); i++) validate(inp.args[i]);
    return inp;
}

Node preprocess(Node inp) {
    std::vector<Node> args;
    args.push_back(inp);
    return astnode("outer", args, inp.metadata);
}

Node rewrite(Node inp) {
    return optimize(apply_rules(validate(preprocess(inp))));
}

using namespace std;
