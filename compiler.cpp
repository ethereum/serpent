#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"
#include "bignum.h"
#include "opcodes.h"

struct programData {
    std::map<std::string, std::string> vars;
    bool allocUsed;
    bool calldataUsed;
    std::vector<Node> code;
};

programData pd() {
    programData o;
    o.allocUsed = false;
    o.calldataUsed = false;
    return o;
}

programData merge(programData existing, programData newdata) {
    for (std::map<std::string,std::string>::iterator it = newdata.vars.begin();
         it != newdata.vars.end();
         it++) {
        existing.vars[(*it).first] = (*it).second;
    }
    existing.allocUsed = existing.allocUsed || newdata.allocUsed;
    existing.calldataUsed = existing.calldataUsed || newdata.calldataUsed;
    existing.code = newdata.code;
    return existing;
}

programData flatten(Node node, programData pdata=pd()) {
    std::string symb = "_"+mkUniqueToken();
    if (node.type == TOKEN) {
        pdata.code.push_back(nodeToNumeric(node));
    }
    else if (node.val == "ref" || node.val == "get" || node.val == "set") {
        std::string varname = node.args[0].val;
        if (!pdata.vars.count(varname)) {
            pdata.vars[varname] = intToDecimal(pdata.vars.size() * 32);
        }
        if (varname == "msg.data") pdata.calldataUsed = true;
        if (node.val == "set") {
            pdata = merge(pdata, flatten(node.args[1], pdata));
        }
        pdata.code.push_back(token(pdata.vars[varname], node.metadata));
        if (node.val == "set") {
             pdata.code.push_back(token("MSTORE", node.metadata));
        }
        else if (node.val == "get") {
             pdata.code.push_back(token("MLOAD", node.metadata));
        }
    }
    else if (node.val == "seq") {
        for (int i = 0; i < node.args.size(); i++) {
            pdata = merge(pdata, flatten(node.args[i], pdata));
        }
    }
    else if (node.val == "unless" && node.args.size() == 2) {
        pdata = merge(pdata, flatten(node.args[0], pdata));
        pdata.code.push_back(token("$endif"+symb, node.metadata));
        pdata.code.push_back(token("JUMPI", node.metadata));
        pdata = merge(pdata, flatten(node.args[1], pdata));
        pdata.code.push_back(token("~endif"+symb, node.metadata));
    }
    else if (node.val == "if" && node.args.size() == 3) {
        pdata = merge(pdata, flatten(node.args[0], pdata));
        pdata.code.push_back(token("NOT", node.metadata));
        pdata.code.push_back(token("$else"+symb, node.metadata));
        pdata.code.push_back(token("JUMPI", node.metadata));
        pdata = merge(pdata, flatten(node.args[1], pdata));
        pdata.code.push_back(token("$endif"+symb, node.metadata));
        pdata.code.push_back(token("JUMP", node.metadata));
        pdata.code.push_back(token("~else"+symb, node.metadata));
        pdata = merge(pdata, flatten(node.args[2], pdata));
        pdata.code.push_back(token("~endif"+symb, node.metadata));
    }
    else if (node.val == "until") {
        pdata.code.push_back(token("~beg"+symb, node.metadata));
        pdata = merge(pdata, flatten(node.args[0], pdata));
        pdata.code.push_back(token("$end"+symb, node.metadata));
        pdata.code.push_back(token("JUMPI", node.metadata));
        pdata = merge(pdata, flatten(node.args[1], pdata));
        pdata.code.push_back(token("$beg"+symb, node.metadata));
        pdata.code.push_back(token("JUMP", node.metadata));
        pdata.code.push_back(token("~end"+symb, node.metadata));
    }
    else if (node.val == "lll") {
        std::string LEN = "$begincode"+symb+".endcode"+symb;
        pdata.code.push_back(token(LEN, node.metadata));
        pdata.code.push_back(token("DUP", node.metadata));
        pdata = merge(pdata, flatten(node.args[1], pdata));
        pdata.code.push_back(token("$begincode"+symb, node.metadata));
        pdata.code.push_back(token("CODECOPY", node.metadata));
        pdata.code.push_back(token("$endcode"+symb, node.metadata));
        pdata.code.push_back(token("JUMP", node.metadata));
        pdata.code.push_back(token("~begincode"+symb, node.metadata));
        pdata.code.push_back(token("#CODE_BEGIN", node.metadata));
        pdata = merge(pdata, flatten(node.args[0], pdata));
        pdata.code.push_back(token("#CODE_END", node.metadata));
        pdata.code.push_back(token("~endcode"+symb, node.metadata));
    }
    else if (node.val == "alloc") {
        pdata.allocUsed = true;
        pdata = merge(pdata, flatten(node.args[0], pdata));
        pdata.code.push_back(token("MSIZE", node.metadata));
        pdata.code.push_back(token("SWAP", node.metadata));
        pdata.code.push_back(token("MSIZE", node.metadata));
        pdata.code.push_back(token("ADD", node.metadata));
        pdata.code.push_back(token("1", node.metadata));
        pdata.code.push_back(token("SWAP", node.metadata));
        pdata.code.push_back(token("SUB", node.metadata));
        pdata.code.push_back(token("0", node.metadata));
        pdata.code.push_back(token("SWAP", node.metadata));
        pdata.code.push_back(token("MSTORE8", node.metadata));
    }
    else if (node.val == "array_lit") {
        pdata.allocUsed = true;
        pdata.code.push_back(token("MSIZE", node.metadata));
        if (node.args.size()) {
            pdata.code.push_back(token("DUP", node.metadata));
            for (int i = 0; i < node.args.size(); i++) {
                pdata = merge(pdata, flatten(node.args[i], pdata));
                pdata.code.push_back(token("SWAP", node.args[i].metadata));
                pdata.code.push_back(token("MSTORE", node.args[i].metadata));
                pdata.code.push_back(token("DUP", node.args[i].metadata));
                pdata.code.push_back(token("32", node.args[i].metadata));
                pdata.code.push_back(token("ADD", node.args[i].metadata));
            }
            pdata.code.pop_back();
            pdata.code.pop_back();
            pdata.code.pop_back();
        }
    }
    else {
        for (int i = node.args.size() - 1; i >= 0; i--) {
            pdata = merge(pdata, flatten(node.args[i], pdata));
        }
        pdata.code.push_back(token(node.val, node.metadata));
    }
    return pdata;
}

std::vector<Node> finalize(programData c) {
    std::vector<Node> newCode;
    if (c.allocUsed && c.vars.size() > 0) {
        newCode.push_back(token("0", c.code[0].metadata));
        std::string lastMem = intToDecimal(c.vars.size() * 32 - 1);
        newCode.push_back(token(lastMem, c.code[0].metadata));
        newCode.push_back(token("MSTORE8", c.code[0].metadata));
    }
    if (c.calldataUsed) {
        newCode.push_back(token("MSIZE", c.code[0].metadata));
        newCode.push_back(token("CALLDATASIZE", c.code[0].metadata));
        newCode.push_back(token("MSIZE", c.code[0].metadata));
        newCode.push_back(token("0", c.code[0].metadata));
        newCode.push_back(token("CALLDATACOPY", c.code[0].metadata));
        newCode.push_back(token(c.vars["msg.data"], c.code[0].metadata));
        newCode.push_back(token("MSTORE", c.code[0].metadata));
    }
    for (int i = 0; i < c.code.size(); i++) newCode.push_back(c.code[i]);
    return newCode;
}

std::vector<Node> compile_lll(Node node) {
    return finalize(flatten(node));
}

std::vector<Node> dereference(std::vector<Node> program) {
    int labelLength = 1;
    int tokenCount = program.size();
    while (tokenCount > 64) {
        tokenCount /= 256;
        labelLength += 1;
    }
    std::vector<Node> iq;
    for (int i = program.size() - 1; i >= 0; i--) iq.push_back(program[i]);
    std::map<std::string,std::string> labelMap;
    int pos = 0;
    std::vector<int> codeStk;
    codeStk.push_back(0);
    std::vector<Node> mq;
    while (iq.size()) {
        Node front = iq.back();
        iq.pop_back();
        if (!isNumberLike(front) && front.val[0] == '~') {
            labelMap[front.val.substr(1)] = intToDecimal(pos - codeStk.back());
        }
        else if (front.val == "#CODE_BEGIN") codeStk.push_back(pos);
        else if (front.val == "#CODE_END") codeStk.pop_back();
        else {
            mq.push_back(front);
            if (isNumberLike(front)) {
                pos += 1 + toByteArr(front.val, front.metadata).size();
            }
            else if (front.val[0] == '$') {
                pos += labelLength + 1;
            }
            else pos += 1;
        }
    }
    std::vector<Node> oq;
    for (int i = 0; i < mq.size(); i++) {
        Node m = mq[i];
        if (isNumberLike(m)) {
            m = nodeToNumeric(m);
            std::vector<Node> os = toByteArr(m.val, m.metadata);
            int len = os.size();
            oq.push_back(token("PUSH"+intToDecimal(len), m.metadata));
            for (int i = 0; i < os.size(); i++) oq.push_back(os[i]);
        }
        else if (m.val[0] == '$') {
            std::vector<Node> os;
            int dotLoc = m.val.find('.');
            if (dotLoc == -1) {
                std::string tokStr = "PUSH"+intToDecimal(labelLength);
                oq.push_back(token(tokStr, m.metadata));
                os = toByteArr(labelMap[m.val.substr(1)], m.metadata);
            }
            else {
                std::string tokStr = "PUSH"+intToDecimal(labelLength);
                oq.push_back(token(tokStr, m.metadata));
                std::string start = labelMap[m.val.substr(1, dotLoc - 1)],
                            end = labelMap[m.val.substr(dotLoc + 1)],
                            dist = decimalSub(end, start);
                os = toByteArr(dist, m.metadata);
            }
            for (int i = 0; i < os.size(); i++) oq.push_back(os[i]);
        }
        else oq.push_back(m);
    }
    return oq;
}

std::string serialize(std::vector<Node> derefed) {
    std::string o = "";
    for (int i = 0; i < derefed.size(); i++) {
        int v;
        if (isNumberLike(derefed[i])) {
            v = decimalToInt(derefed[i].val);
        }
        else if (derefed[i].val.substr(0,4) == "PUSH") {
            v = 95 + decimalToInt(derefed[i].val.substr(4));
        }
        else {
            v = opcode(derefed[i].val);
        }
        o += std::string("0123456789abcdef").substr(v/16, 1);
        o += std::string("0123456789abcdef").substr(v%16, 1);
    }
    return o;
}

std::string assemble(std::vector<Node> program) {
    return serialize(dereference(program));
}

/*int main() {
    Node n = rewrite(parseSerpent("x = 2 + 3"));
    cout << printAST(n) << " r\n";
    programData c = compile(n);
    cout << printTokens(c.code) << " c\n";
    c = finalize(c);
    cout << printTokens(c.code) << " f\n";
    std::vector<Node> d = dereference(c);
    cout << printTokens(d) << " d\n";
    std::string s = serialize(d);
    cout << s << " s\n";
    cout << printTokens(compile(rewrite(parseSerpent("if contract.storage[msg.data[0]]:\n    contract.storage[msg.data[0]] = msg.data[1]"))).code) << "\n";
    cout << printTokens(dereference(finalize(compile(rewrite(parseSerpent("if contract.storage[msg.data[0]]:\n    contract.storage[msg.data[0]] = msg.data[1]")))))) << "\n";
    cout << serialize(dereference(finalize(compile(rewrite(parseSerpent("if contract.storage[msg.data[0]]:\n    contract.storage[msg.data[0]] = msg.data[1]")))))) << "\n";
}*/
