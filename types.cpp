#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>

const int TOKEN = 0,
          ASTNODE = 1,
          SPACE = 2,
          BRACK = 3,
          SQUOTE = 4,
          DQUOTE = 5,
          SYMB = 6,
          ALPHANUM = 7,
          LPAREN = 8,
          RPAREN = 9,
          COMMA = 10,
          COLON = 11,
          UNARY_OP = 12,
          BINARY_OP = 13,
          COMPOUND = 14;

// Stores metadata about each token
struct Metadata {
    std::string file;
    int ln;
    int ch;
};

Metadata metadata(std::string file="main", int ln=0, int ch=0) {
    Metadata o;
    o.file = file;
    o.ln = ln;
    o.ch = ch;
    return o;
}

// type can be TOKEN or ASTNODE
struct Node {
    int type;
    std::string val;
    std::vector<Node> args;
    Metadata metadata;
};

Node token(std::string val, Metadata metadata) {
    Node o;
    o.type = 0;
    o.val = val;
    o.metadata = metadata;
    return o;
}

Node astnode(std::string val, std::vector<Node> args, Metadata metadata) {
    Node o;
    o.type = 1;
    o.val = val;
    o.args = args;
    o.metadata = metadata;
    return o;
}

// Print token list
std::string printTokens(std::vector<Node> tokens) {
    if (!tokens.size()) return "[]";
    std::string s = "[";
    for (int i = 0; i < tokens.size(); i++) {
        s += tokens[i].val + ", ";
    }
    return s.substr(0,s.length() - 2) + "]";
}

// Prints a lisp AST on one line
std::string printSimple(Node ast) {
    if (ast.type == TOKEN) return ast.val;
    std::string o = "(" + ast.val;
    std::vector<std::string> subs;
    for (int i = 0; i < ast.args.size(); i++) {
        o += " " + printSimple(ast.args[i]);
    }
    return o + ")";
}

bool isNumberLike(Node node) {
    if (node.type == ASTNODE) return false;
    if (node.val[0] == '"' && node.val[node.val.length()-1] == '"') {
        return true;
    }
    if (node.val[0] == '\'' && node.val[node.val.length()-1] == '\'') {
        return true;
    }
    if (node.val.substr(0,2) == "0x") return true;
    bool isPureNum = true;
    for (int i = 0; i < node.val.length(); i++) {
        isPureNum = isPureNum && node.val[i] >= '0' && node.val[i] <= '9';
    }
    return isPureNum;
}

std::string nums = "0123456789";

std::string intToDecimal(int branch) {
    if (branch < 10) return nums.substr(branch, 1);
    else return intToDecimal(branch / 10) + nums.substr(branch % 10,1);
}

std::string decimalAdd(std::string a, std::string b) {
    std::string o = a;
    while (b.length() < a.length()) b = "0" + b;
    while (o.length() < b.length()) o = "0" + o;
    bool carry = false;
    for (int i = o.length() - 1; i >= 0; i--) {
        o[i] = o[i] + b[i] - '0';
        if (carry) o[i]++;
        if (o[i] > '9') {
            o[i] -= 10;
            carry = true;
        }
        else carry = false;
    }
    if (carry) o = "1" + o;
    return o;
}

std::string decimalDigitMul(std::string a, int dig) {
    if (dig == 0) return "0";
    else return decimalAdd(a, decimalDigitMul(a, dig - 1));
}

std::string decimalMul(std::string a, std::string b) {
    std::string o = "0";
    for (int i = 0; i < b.length(); i++) {
        std::string n = decimalDigitMul(a, b[i] - '0');
        for (int j = i + 1; j < b.length(); j++) n += "0";
        o = decimalAdd(o, n);
    }
    return o;
}

std::string decimalSub(std::string a, std::string b) {
    while (b.length() < a.length()) b = "0" + b;
    std::string c = b;
    for (int i = 0; i < c.length(); i++) c[i] = '0' + ('9' - c[i]);
    std::string o = decimalAdd(decimalAdd(a, c).substr(1), "1");
    while (o.size() > 1 && o[0] == '0') o = o.substr(1);
    return o;
}

std::string decimalDiv(std::string a, std::string b) {
    std::string c = b;
    if (a.size() <= c.size() && (a.size() < c.size() || a < c)) return "0";
    int zeroes = -1;
    while (a.size() >= c.size() && (a.size() > c.size() || a >= c)) {
        zeroes += 1;
        c = c + "0";
    }
    c = c.substr(0, c.size() - 1);
    std::string quot = "0";
    while (a.size() >= c.size() && (a.size() > c.size() || a >= c)) {
        a = decimalSub(a, c);
        quot = decimalAdd(quot, "1");
    }
    for (int i = 0; i < zeroes; i++) quot += "0";
    return decimalAdd(quot, decimalDiv(a, b));
}

std::string decimalMod(std::string a, std::string b) {
    return decimalSub(a, decimalMul(decimalDiv(a, b), b));
}

std::vector<Node> toByteArr(std::string val, Metadata metadata) {
    std::vector<Node> o;
    if (val == "0") o.push_back(token("0", metadata));
    while (val != "0") {
        o.push_back(token(decimalMod(val, "256"), metadata));
        val = decimalDiv(val, "256");
    }
    std::vector<Node> o2;
    for (int i = o.size() - 1; i >= 0; i--) o2.push_back(o[i]);
    return o2;
}

int decimalToInt(std::string a) {
    if (a.size() == 0) return 0;
    else return (a[a.size() - 1] - '0') 
        + decimalToInt(a.substr(0,a.size()-1)) * 10;
}

int decimalLog256(std::string a) {
    int p = 0;
    std::string o = "1";
    while (o.size() <= a.size() && (o.size() < a.size() || o < a)) {
        p += 1;
        o = decimalMul(o, "256");
    }
    return p;
}

Node nodeToNumeric(Node node) {
    std::string o = "0";
    if (node.val[0] == '"' && node.val[node.val.length()-1] == '"') {
        for (int i = 1; i < node.val.length() - 1; i++) {
            o = decimalAdd(decimalMul(o,"256"), intToDecimal(node.val[i]));
        }
    }
    else if (node.val.substr(0,2) == "0x") {
        for (int i = 1; i < node.val.length() - 1; i++) {
            int dig = std::string("0123456789abcdef").find(node.val[i]);
            o = decimalAdd(decimalMul(o,"16"), intToDecimal(dig));
        }
    }
    else o = node.val;
    return token(o, node.metadata);
}

int counter = 0;
