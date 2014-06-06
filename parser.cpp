#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "types.cpp"

// These appear as independent tokens even if inside a stream of symbols
const std::string atoms[] = { "#", "-", "//", "(", ")", "[", "]", "{", "}" };
const int numAtoms = 9;

int chartype(char c) {
    if (c >= '0' && c <= '9') return ALPHANUM;
    else if (c >= 'a' && c <= 'z') return ALPHANUM;
    else if (c >= 'A' && c <= 'Z') return ALPHANUM;
    else if (std::string("._@").find(c) != -1) return ALPHANUM;
    else if (c == '\t' || c == ' ' || c == '\n') return SPACE;
    else if (std::string("()[]{}").find(c) != -1) return BRACK;
    else if (c == '"') return DQUOTE;
    else if (c == '\'') return SQUOTE;
    else return SYMB;
}

// "y = f(45,124)/3" -> [ "y", "f", "(", "45", ",", "124", ")", "/", "3"]
std::vector<Node> tokenize(std::string inp, Metadata metadata=metadata()) {
    int curtype = SPACE;
    int pos = 0;
    metadata.ch = 0;
    std::string cur;
    std::vector<Node> out;

    inp += " ";
    while (pos < inp.length()) {
        int headtype = chartype(inp[pos]);
        // Are we inside a quote?
        if (curtype == SQUOTE || curtype == DQUOTE) {
            // Close quote
            if (headtype == curtype) {
                cur += inp[pos];
                out.push_back(token(cur, metadata));
                cur = "";
                metadata.ch = pos;
                curtype = SPACE;
            }
            // eg. \xc3
            else if (inp.length() >= pos + 4 && inp.substr(pos, 2) == "\\x") {
                cur += (std::string("0123456789abcdef").find(inp[pos+2]) * 16
                        + std::string("0123456789abcdef").find(inp[pos+3]));
                pos += 4;
            }
            // Newline
            else if (inp.substr(pos, 2) == "\\n") {
                cur += '\n';
                pos += 2;
            }
            // Backslash escape
            else if (inp.length() >= pos + 2 && inp[pos] == '\\') {
                cur += inp[pos + 1];
                pos += 2;
            }
            // Normal character
            else {
                cur += inp[pos];
                pos += 1;
            }
        }
        else {
            // Handle atoms ( '//', '#', '-', brackets )
            for (int i = 0; i < numAtoms; i++) {
                int split = cur.length() - atoms[i].length();
                if (split >= 0 && cur.substr(split) == atoms[i]) {
                    if (split > 0) {
                        out.push_back(token(cur.substr(0, split), metadata));
                    }
                    metadata.ch += split;
                    out.push_back(token(cur.substr(split), metadata));
                    metadata.ch = pos;
                    cur = "";
                    curtype = SPACE;
                }
            }
            // Boundary between different char types
            if (headtype != curtype) {
                if (curtype != SPACE && cur != "") {
                    out.push_back(token(cur, metadata));
                }
                metadata.ch = pos;
                cur = "";
            }
            cur += inp[pos];
            curtype = headtype;
            pos += 1;
        }
    }
    return out;
}

// Extended BEDMAS precedence order
int precedence(Node tok) {
    std::string v = tok.val;
    if (v == "!") return 0;
    else if (v=="^") return 1;
    else if (v=="*" || v=="/" || v=="$/" || v=="%" | v=="$%") return 2;
    else if (v=="+" || v=="-") return 3;
    else if (v=="<" || v==">" || v=="<=" || v==">=") return 4;
    else if (v=="&" || v=="|" || v=="xor" || v=="==") return 5;
    else if (v=="&&" || v=="and") return 6;    
    else if (v=="||" || v=="or") return 7;
    else if (v=="=") return 10;
    else return -1;
}

// Token classification for shunting-yard purposes
int toktype(Node tok) {
    if (tok.type == ASTNODE) return COMPOUND;
    std::string v = tok.val;
    if (v == "(" || v == "[") return LPAREN;
    else if (v == ")" || v == "]") return RPAREN;
    else if (v == ",") return COMMA;
    else if (v == ":") return COLON;
    else if (v == "!") return UNARY_OP;
    else if (precedence(tok) >= 0) return BINARY_OP;
    else return ALPHANUM;
}

struct _parseOutput {
    Node node;
    int newpos;
};

// Helper, returns subtree and position of start of next node
_parseOutput _parse(std::vector<Node> inp, int pos) {
    Metadata met = inp[pos].metadata;
    _parseOutput o;
    // Bracket: keep grabbing tokens until we get to the
    // corresponding closing bracket
    if (inp[pos].val == "(" || inp[pos].val == "[") {
        std::string fun, rbrack;
        std::vector<Node> args;
        pos += 1;
        if (inp[pos].val == "[") {
            fun = "access";
            rbrack = "]";
        }
        else rbrack = ")";
        // First argument is the function
        while (inp[pos].val != ")") {
            _parseOutput po = _parse(inp, pos);
            if (fun.length() == 0 && po.node.type == 1) {
                std::cerr << "Error: first arg must be function\n";
                fun = po.node.val;
            }
            else if (fun.length() == 0) {
                fun = po.node.val;
            }
            else {
                args.push_back(po.node);
            }
            pos = po.newpos;
        }
        o.newpos = pos + 1;
        o.node = astnode(fun, args, met);
    }
    // Normal token, return it and advance to next token
    else {
        o.newpos = pos + 1;
        o.node = token(inp[pos].val, met);
    }
    return o;
}

// stream of tokens -> lisp parse tree
Node parseTokenStream(std::vector<Node> inp) {
    _parseOutput o = _parse(inp, 0);
    return o.node;
}

// Converts to reverse polish notation
std::vector<Node> shuntingYard(std::vector<Node> tokens) {
    std::vector<Node> iq;
    for (int i = tokens.size() - 1; i >= 0; i--) {
        iq.push_back(tokens[i]);
    }
    std::vector<Node> oq;
    std::vector<Node> stack;
    Node prev, tok;
    int prevtyp, toktyp;
    
    while (iq.size()) {
        prev = tok;
        prevtyp = toktyp;
        tok = iq.back();
        toktyp = toktype(tok);
        iq.pop_back();
        // Alphanumerics go straight to output queue
        if (toktyp == ALPHANUM) {
            oq.push_back(tok);
        }
        // Left parens go on stack and output queue
        else if (toktyp == LPAREN) {
            if (prevtyp != ALPHANUM && prevtyp != RPAREN) {
                oq.push_back(token("id", tok.metadata));
            }
            Node fun = oq.back();
            oq.pop_back();
            oq.push_back(tok);
            oq.push_back(fun);
            stack.push_back(tok);
        }
        // If rparen, keep moving from stack to output queue until lparen
        else if (toktyp == RPAREN) {
            while (stack.size() && toktype(stack.back()) != LPAREN) {
                oq.push_back(stack.back());
                stack.pop_back();
            }
            if (stack.size()) stack.pop_back();
            oq.push_back(tok);
        }
        // If binary op, keep popping from stack while higher bedmas precedence
        else if (toktyp == UNARY_OP || toktyp == BINARY_OP) {
            if (tok.val == "-" && prevtyp != ALPHANUM && prevtyp != RPAREN) {
                oq.push_back(token("0", tok.metadata));
            }
            int prec = precedence(tok);
            while (stack.size() 
                  && toktype(stack.back()) == BINARY_OP 
                  && precedence(stack.back()) <= prec) {
                oq.push_back(stack.back());
                stack.pop_back();
            }
            stack.push_back(tok);
        }
        // Comma and colon mean finish evaluating the argument
        else if (toktyp == COMMA || toktyp == COLON) {
            while (stack.size() && toktype(stack.back()) != LPAREN) {
                oq.push_back(stack.back());
                stack.pop_back();
            }
            if (toktyp == COLON) oq.push_back(tok);
        }
    }
    while (stack.size()) {
        oq.push_back(stack.back());
        stack.pop_back();
    }
    return oq;
}

// Converts reverse polish notation into tree
Node treefy(std::vector<Node> stream) {
    std::vector<Node> iq;
    for (int i = stream.size() -1; i >= 0; i--) {
        iq.push_back(stream[i]);
    }
    std::vector<Node> oq;
    while (iq.size()) {
        Node tok = iq.back();
        iq.pop_back();
        int typ = toktype(tok);
        // If unary, take node off end of oq and wrap it with the operator
        // If binary, do the same with two nodes
        if (typ == UNARY_OP || typ == BINARY_OP) {
            std::vector<Node> args;
            int rounds = (typ == BINARY_OP) ? 2 : 1;
            for (int i = 0; i < rounds; i++) {
                args.push_back(oq.back());
                oq.pop_back();
            }
            std::vector<Node> args2;
            while (args.size()) {
                args2.push_back(args.back());
                args.pop_back();
            }
            oq.push_back(astnode(tok.val, args2, tok.metadata));
        }
        // If rparen, keep grabbing until we get to an lparen
        else if (toktype(tok) == RPAREN) {
            std::vector<Node> args;
            while (toktype(oq.back()) != LPAREN) {
                args.push_back(oq.back());
                oq.pop_back();
            }
            oq.pop_back();
            // We represent a[b] as (access a b)
            if (tok.val == "]") args.push_back(token("access", tok.metadata));
            std::string fun = args.back().val;
            args.pop_back();
            // We represent [1,2,3] as (array_lit 1 2 3)
            if (fun == "access" && args.size() && args.back().val == "id") {
                fun = "array_lit";
                args.pop_back();
            }
            std::vector<Node> args2;
            while (args.size()) {
                args2.push_back(args.back());
                args.pop_back();
            }
            // When evaluating 2 + (3 * 5), the shunting yard algo turns that
            // into 2 ( id 3 5 * ) +, effectively putting "id" as a dummy
            // function where the algo was expecting a function to call the
            // thing inside the brackets. This reverses that step
            if (fun == "id") {
                fun = args[0].val;
                args = args[0].args;
            }
            oq.push_back(astnode(fun, args2, tok.metadata));
        }
        else oq.push_back(tok);
    }
    // Output must have one argument
    if (oq.size() == 0) {
        std::cerr << "Output blank!\n";
    }
    else if (oq.size() > 1) {
        std::cerr << "Too many tokens in output: ";
        for (int i = 0; i < oq.size(); i++)
            std::cerr << printSimple(oq[i]) << "\n";
    }
    else return oq[0];
}

// Parses LLL
Node parseLLL(std::string s, Metadata metadata=metadata()) {
    return parseTokenStream(tokenize(s, metadata));
}

// Parses one line of serpent
Node parseLine(std::string s, Metadata metadata) {
    return treefy(shuntingYard(tokenize(s, metadata)));
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

// Count spaces at beginning of line
int spaceCount(std::string s) {
    int pos = 0;
    while (pos < s.length() && (s[pos] == ' ' || s[pos] == '\t')) pos += 1;
    return pos;
}

// Is it a bodied command?
bool bodied(std::string tok) {
    return tok == "if" || tok == "elif";
}

// Are the two commands meant to continue each other? 
bool bodiedContinued(std::string prev, std::string tok) {
    return prev == "if" && tok == "elif" 
        || prev == "elif" && tok == "else"
        || prev == "elif" && tok == "elif"
        || prev == "if" && tok == "else"
        || prev == "init" && tok == "code"
        || prev == "shared" && tok == "init";
}

// Parse lines of serpent (helper function)
Node parseLines(std::vector<std::string> lines, Metadata metadata, int sp) {
    std::vector<Node> o;
    int origLine = metadata.ln;
    int i = 0;
    while (i < lines.size()) {
        std::string main = lines[i];
        int spaces = spaceCount(main);
        if (spaces != sp) {
            std::cerr << "Indent mismatch on line " << (origLine+i) << "\n";
        }
        int lineIndex = i;
        metadata.ln = origLine + i; 
        // Tokenize current line
        std::vector<Node> tokens = tokenize(main.substr(sp), metadata);
        // Remove extraneous tokens, including if / elif
        std::vector<Node> tokens2;
        for (int j = 0; j < tokens.size(); j++) {
            if (tokens[j].val == "#" || tokens[j].val == "//") break;
            if (j == tokens.size() - 1 && tokens[j].val == ":") break;
            if (j >= 1 || !bodied(tokens[j].val)) {
                tokens2.push_back(tokens[j]);
            }
        }
        // Is line empty or comment-only?
        if (tokens2.size() == 0) {
            i += 1;
            continue;
        }
        // Parse current line
        Node out = treefy(shuntingYard(tokens2));
        // Parse child block
        int indent = 999999;
        std::vector<std::string> childBlock;
        while (1) {
            i += 1;
            if (i >= lines.size() || spaceCount(lines[i]) <= sp) break;
            childBlock.push_back(lines[i]);
        }
        // Bring back if / elif into AST
        if (bodied(tokens[0].val)) {
            std::vector<Node> args;
            args.push_back(out);
            out = astnode(tokens[0].val, args, out.metadata);
        }
        // Add child block to AST
        if (childBlock.size()) {
            int childSpaces = spaceCount(childBlock[0]);
            out.type = ASTNODE;
            out.args.push_back(parseLines(childBlock, metadata, childSpaces));
        }
        if (o.size() == 0 || o.back().type == TOKEN) {
            o.push_back(out);
            continue;
        }
        // This is a little complicated. Basically, the idea here is to build
        // constructions like [if [< x 5] [a] [elif [< x 10] [b] [else [c]]]]
        std::vector<Node> u;
        u.push_back(o.back());
        if (bodiedContinued(o.back().val, out.val)) {
            while (bodiedContinued(u.back().val, out.val)) {
                u.push_back(u.back().args.back());
            }
            u.pop_back();
            u.back().args.push_back(out);
            while (u.size() > 1) {
                Node v = u.back();
                u.pop_back();
                u.back().args.pop_back();
                u.back().args.push_back(v);
            }
            o.pop_back();
            o.push_back(u[0]);
        }
        else o.push_back(out);
    }
    if (o.size() == 1) return o[0];
    else return astnode("seq", o, o[0].metadata);
}

// Parses serpent code
Node parseSerpent(std::string s, std::string file="file") {
    return parseLines(splitLines(s), metadata(file, 0, 0), 0);
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

// Prints a lisp AST
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

using namespace std;

/*int main() {
    cout << printTokens(tokenize("f(2,5+3)/45+-7")) << "\n";
    cout << printAST(parseLLL("(seq (+ 2 7) (- 2 (* 7 4)) (mstore (mload 6)))")) << "\n";
    cout << printAST(parseLLL("(/ (+ 2 3) (mload (mstore 3214 ) 124124 ) 128461274672164124124 (+ 8 9) (/ 1267416274 12431241 21412412 4 ))")) << "\n";
    cout << printAST(parseLLL(printAST(parseLLL("(/ (+ 2 3) (mload (mstore 3214 ) 124124 ) 128461274672164124124 (+ 8 9) (/ 1267416274 12431241 21412412 4 ))")))) << "\n";
    cout << printTokens(shuntingYard(tokenize("2 + 3"))) << "\n";
    cout << printTokens(shuntingYard(tokenize("2 / query(4 + 3) * [1,2,3]"))) << "\n";
    cout << printAST(parseLine("2 + 3", metadata())) << "\n";
    cout << printAST(parseLine("bob[5] = \"dog\"", metadata())) << "\n";
    cout << printAST(parseLine("2 / query(4 + 3) * [1,2,3]", metadata())) << "\n";
    cout << printAST(parseSerpent("x = 2 + 3")) << "\n";
    cout << printAST(parseSerpent("x = 2 + 3\ny = 5 * 7")) << "\n";
    cout << printAST(parseSerpent("if x < 2 + 3:\n    y = 5 * 7")) << "\n";
    cout << printAST(parseSerpent("if x < 2 + 3:\n    y = 5 * 7\nelif x < 9:\n    y = 15")) << "\n";
    cout << printAST(parseSerpent("if x < 2 + 3:\n    y = 5 * 7\nelif x < 9:\n    y = 15\nelif x < f(4):\n    y = -4\nelse:\n    y = 0")) << "\n";
    cout << printAST(parseSerpent("if contract.storage[msg.data[0]]:\n    contract.storage[msg.data[0]] = msg.data[1]\n    return(1)\nelse:\n    return(0)")) << "\n";
}*/
