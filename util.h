#ifndef ETHSERP_UTIL
#define ETHSERP_UTIL

#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include <cerrno>
#include <stdint.h>

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
          COMPOUND = 14,
          TOKEN_SPLITTER = 15;

const int STATIC = 16,
          ARRAY = 17,
          BYTES = 18;

// Stores metadata about each token
class Metadata {
    public:
        Metadata(std::string File="main", int Ln=-1, int Ch=-1) {
            file = File;
            ln = Ln;
            ch = Ch;
            fixed = false;
        }
        std::string file;
        int ln;
        int ch;
        bool fixed;
};

std::string mkUniqueToken();

// type can be TOKEN or ASTNODE
class Node {
    public:
        int type;
        std::string val;
        std::vector<Node> args;
        Metadata metadata;
};
Node token(std::string val, Metadata met=Metadata());
Node astnode(std::string val, std::vector<Node> args, Metadata met=Metadata());
Node astnode(std::string val, Metadata met=Metadata());
Node astnode(std::string val, Node a, Metadata met=Metadata());
Node astnode(std::string val, Node a, Node b, Metadata met=Metadata());
Node astnode(std::string val, Node a, Node b, Node c, Metadata met=Metadata());
Node astnode(std::string val, Node a, Node b,
             Node c, Node d, Metadata met=Metadata());

// Number of tokens in a tree
int treeSize(Node prog);

// Print token list
std::string printTokens(std::vector<Node> tokens);

// Prints a lisp AST on one line
std::string printSimple(Node ast);

// Pretty-prints a lisp AST
std::string printAST(Node ast, bool printMetadata=false);

// Splits text by line
std::vector<std::string> splitLines(std::string s);

// Inverse of splitLines
std::string joinLines(std::vector<std::string> lines);

// Indent all lines by 4 spaces
std::string indentLines(std::string inp);

// Converts binary to simple numeric format
std::string binToNumeric(std::string inp);

// Converts string to list of bytes
std::vector<uint8_t> strToBytes(std::string inp);

// Converts string to simple numeric format
std::string strToNumeric(std::string inp, int strpad);

// Does the node contain a number (eg. 124, 0xf012c, "george")
bool isNumberLike(Node node);

//Normalizes number representations
Node nodeToNumeric(Node node);

//If a node is numeric, normalize its representation
Node tryNumberize(Node node);

//Converts a value to an array of byte number nodes
std::vector<Node> toByteArr(std::string val, Metadata metadata, int minLen=1);

//Reads a file
std::string get_file_contents(std::string filename);

//Does a file exist?
bool exists(std::string fileName);

//Report error
void err(std::string errtext, Metadata met);

//Report warning
void warn(std::string errtext, Metadata met);

//Bin to hex
std::string binToHex(std::string inp);

//Hex to bin
std::string hexToBin(std::string inp);

//Lower to upper
std::string upperCase(std::string inp);

//Three-int vector
std::vector<int> triple(int a, int b, int c);

//Empty hash
std::vector<uint8_t> zeroes(int n);

//Empty boolean vector
std::vector<bool> falses(int n);

//Extend node vector
std::vector<Node> extend(std::vector<Node> a, std::vector<Node> b);

//Converts a byte array into a value
std::string bytesToDecimal(std::vector<uint8_t> b);

// Is the number decimal?
bool isDecimal(std::string inp);

#define asn astnode
#define tkn token
#define msi std::map<std::string, int>
#define msn std::map<std::string, Node>
#define mss std::map<std::string, std::string>
#define strvec std::vector<std::string>

template <typename T, typename U>
class create_map
{
private:
    std::map<T, U> m_map;
public:
    create_map(const T& key, const U& val)
    {
        m_map[key] = val;
    }

    create_map<T, U>& operator()(const T& key, const U& val)
    {
        m_map[key] = val;
        return *this;
    }

    operator std::map<T, U>()
    {
        return m_map;
    }
};

#endif
