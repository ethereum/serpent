#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"
#include "lllparser.h"
#include "bignum.h"
#include "optimize.h"

// Compile-time arithmetic calculations
Node calcArithmetic(Node inp, bool modulo=true) {
    if (inp.type == TOKEN) {
        Node o = tryNumberize(inp);
        if (decimalGt(o.val, tt256, true))
            err("Value too large (exceeds 32 bytes or 2^256)", inp.metadata);
        return o;
    }
	for (unsigned i = 0; i < inp.args.size(); i++) {
        inp.args[i] = calcArithmetic(inp.args[i]);
    }
    // Arithmetic-specific transform
    if (inp.val == "+") inp.val = "add";
    if (inp.val == "*") inp.val = "mul";
    if (inp.val == "-") inp.val = "sub";
    if (inp.val == "/") inp.val = "sdiv";
    if (inp.val == "^") inp.val = "exp";
    if (inp.val == "**") inp.val = "exp";
    if (inp.val == "%") inp.val = "smod";
    // Degenerate cases for add and mul
    if (inp.args.size() == 2) {
        if (inp.val == "add" && inp.args[0].type == TOKEN && 
                inp.args[0].val == "0") {
            Node x = inp.args[1];
            inp = x;
        }
        if (inp.val == "add" && inp.args[1].type == TOKEN && 
                inp.args[1].val == "0") {
            Node x = inp.args[0];
            inp = x;
        }
        if (inp.val == "mul" && inp.args[0].type == TOKEN && 
                inp.args[0].val == "1") {
            Node x = inp.args[1];
            inp = x;
        }
        if (inp.val == "mul" && inp.args[1].type == TOKEN && 
                inp.args[1].val == "1") {
            Node x = inp.args[0];
            inp = x;
        }
    }
    // Arithmetic computation
    if (inp.args.size() == 2 
            && inp.args[0].type == TOKEN 
            && inp.args[1].type == TOKEN) {
      std::string o;
      if (inp.val == "add") {
          o = decimalAdd(inp.args[0].val, inp.args[1].val);
          if (modulo) o = decimalMod(o, tt256);
      }
      else if (inp.val == "sub") {
          if (decimalGt(inp.args[0].val, inp.args[1].val, true))
              o = decimalSub(inp.args[0].val, inp.args[1].val);
      }
      else if (inp.val == "mul") {
          o = decimalMul(inp.args[0].val, inp.args[1].val);
          if (modulo) o = decimalMod(o, tt256);
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
      else if (inp.val == "exp") {
          if (modulo) o = decimalModExp(inp.args[0].val, inp.args[1].val, tt256);
          else o = decimalExp(inp.args[0].val, inp.args[1].val);
      }
      if (o.length()) return token(o, inp.metadata);
    }
    return inp;
}

// We maintain a "variable map", determining which variables
// produced inside of with clauses were modified. If a variable
// was not modified, and the initial value is trivial (ie. is
// a number), then we simply replace it everywhere
#define varmap std::map<std::string, Node >
const Node VARIABLE_SET = tkn("__VARIABLE_SET_TOKEN17647152687");

bool isReplaceable(Node inp, std::string var) {
    if (inp.type == TOKEN) {
    }
    else if (inp.val == "set") {
        if (inp.args[0].val == var)
            return false;
    }
    else if (inp.val != "with" || inp.args[0].val != var) {
        for (int i = 0; i < inp.args.size(); i++) {
            if (!isReplaceable(inp.args[i], var)) return false;
        }
    }
    else {
        // eg.
        //
        // (with x 5 (with y z (with x (seq (set x 8) 2) ... ) ) )
        if (isReplaceable(inp.args[1], var)) return false;
    }
    return true;
}

Node filterWithStatements(Node inp, varmap vmap = varmap()) {
    if (inp.type == TOKEN)
        return inp;
    else if (inp.val == "get") {
        if (vmap.count(inp.args[0].val))
            return vmap[inp.args[0].val];
        return inp;
    }
    else if (inp.val != "with") {
        std::vector<Node> newArgs;
        for (int i = 0; i < inp.args.size(); i++) {
            varmap sub = vmap;
            newArgs.push_back(filterWithStatements(inp.args[i], sub));
        }
        return asn(inp.val, newArgs, inp.metadata);
    }
    else {
        inp.args[1] = filterWithStatements(inp.args[1], vmap);
        varmap sub = vmap;
        sub.erase(inp.args[0].val);
        if (isDegenerate(inp.args[1]) && isReplaceable(inp.args[2], inp.args[0].val)) {
            sub[inp.args[0].val] = inp.args[1];
            return filterWithStatements(inp.args[2], sub);
        }
        inp.args[2] = filterWithStatements(inp.args[2], sub);
        return inp;
    }
}

// Optimize a node (now does arithmetic, may do other things)
Node optimize(Node inp) {
    return calcArithmetic(filterWithStatements(inp), true);
    //return calcArithmetic(inp, true);
}

// Is a node degenerate (ie. trivial to calculate) ?
bool isDegenerate(Node n) {
    return optimize(n).type == TOKEN;
}

// Is a node purely arithmetic?
bool isPureArithmetic(Node n) {
    return isNumberLike(calcArithmetic(n));
}
