#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"
#include "bignum.h"
#include <fstream>
#include <cerrno>

//Token metadata constructor
Metadata metadata(std::string file, int ln, int ch) {
    Metadata o;
    o.file = file;
    o.ln = ln;
    o.ch = ch;
    return o;
}

//Token or value node constructor
Node token(std::string val, Metadata met) {
    Node o;
    o.type = 0;
    o.val = val;
    o.metadata = met;
    return o;
}

//AST node constructor
Node astnode(std::string val, std::vector<Node> args, Metadata met) {
    Node o;
    o.type = 1;
    o.val = val;
    o.args = args;
    o.metadata = met;
    return o;
}

// Print token list
std::string printTokens(std::vector<Node> tokens) {
    std::string s = "";
    for (int i = 0; i < tokens.size(); i++) {
        s += tokens[i].val + " ";
    }
    return s;
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

// Pretty-prints a lisp AST
std::string printAST(Node ast) {
    if (ast.type == TOKEN) return ast.val;
    std::string o = "(" + ast.val;
    std::vector<std::string> subs;
    for (int i = 0; i < ast.args.size(); i++) {
        subs.push_back(printAST(ast.args[i]));
    }
    int k = 0;
    std::string out = " ";
    // As many arguments as possible go on the same line as the function,
    // except when seq is used
    while (k < subs.size() && o != "(seq") {
        if (subs[k].find("\n") != -1 || (out + subs[k]).length() >= 80) break;
        out += subs[k] + " ";
        k += 1;
    }
    // All remaining arguments go on their own lines
    if (k < subs.size()) {
        o += out + "\n";
        std::vector<std::string> subsSliceK;
        for (int i = k; i < subs.size(); i++) subsSliceK.push_back(subs[i]);
        o += indentLines(joinLines(subsSliceK));
        o += "\n)";
    }
    else {
        o += out.substr(0, out.size() - 1) + ")";
    }
    return o;
}

// Splits text by line
std::vector<std::string> splitLines(std::string s) {
    int pos = 0;
    int lastNewline = 0;
    std::vector<std::string> o;
    while (pos < s.length()) {
        if (s[pos] == '\n') {
            o.push_back(s.substr(lastNewline, pos - lastNewline));
            lastNewline = pos + 1;
        }
        pos = pos + 1;
    }
    o.push_back(s.substr(lastNewline));
    return o;
}

// Inverse of splitLines
std::string joinLines(std::vector<std::string> lines) {
    std::string o = "\n";
    for (int i = 0; i < lines.size(); i++) {
        o += lines[i] + "\n";
    }
    return o.substr(1, o.length() - 2);
}

// Indent all lines by 4 spaces
std::string indentLines(std::string inp) {
    std::vector<std::string> lines = splitLines(inp);
    for (int i = 0; i < lines.size(); i++) lines[i] = "    "+lines[i];
    return joinLines(lines);
}

// Does the node contain a number (eg. 124, 0xf012c, "george")
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

//Normalizes number representations
Node nodeToNumeric(Node node) {
    std::string o = "0";
    if ((node.val[0] == '"' && node.val[node.val.length()-1] == '"')
            || (node.val[0] == '\'' && node.val[node.val.length()-1] == '\'')) {
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

Node tryNumberize(Node node) {
    if (node.type == TOKEN && isNumberLike(node)) return nodeToNumeric(node);
    return node;
}

//Converts a value to an array of byte number nodes
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

int counter = 0;

//Makes a unique token
std::string mkUniqueToken() {
    counter++;
    return intToDecimal(counter);
}

//Does a file exist? http://stackoverflow.com/questions/12774207
bool exists(std::string fileName) {
    std::ifstream infile(fileName.c_str());
    return infile.good();
}

//Reads a file: http://stackoverflow.com/questions/2602013
std::string get_file_contents(std::string filename)
{
  std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
  if (in)
  {
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  throw(errno);
}

//Report error
void err(std::string errtext, Metadata met) {
    std::cerr << "Error (file \"" << met.file << "\", line " << met.ln
              << ", char " << met.ch << "): " << errtext << "\n";
    std::vector<Node> bob;
    bob[0] = token("bob");
}
