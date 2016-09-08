#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "bignum.h"

//Integer to string conversion
std::string unsignedToDecimal(unsigned branch) {
    if (branch < 10) return nums.substr(branch, 1);
    else return unsignedToDecimal(branch / 10) + nums.substr(branch % 10, 1);
}

//Prepend zeros if needed
static std::string prependZeros(const std::string &source, int how_many) {
    std::string prefix = "";
    for (int i = 0; i < how_many; ++i) {
        prefix += "0";
    }
    return prefix + source;
}

//Add two strings representing decimal values
std::string decimalAdd(const std::string &a, const std::string &b) {
    std::string o = prependZeros(a, b.length() - a.length());
    std::string c = prependZeros(b, a.length() - b.length());
    bool carry = false;
    for (int i = o.length() - 1; i >= 0; i--) {
        o[i] = o[i] + c[i] - '0';
        if (carry) o[i]++;
        carry = false;
        if (o[i] > '9') {
            o[i] -= 10;
            carry = true;
        }
    }
    if (carry) o = "1" + o;
    return o;
}

//Helper function for decimalMul
static std::string decimalDigitMul(const std::string &a, int dig) {
    if (dig == 0) return "0";
    else return decimalAdd(a, decimalDigitMul(a, dig - 1));
}

//Multiply two strings representing decimal values
std::string decimalMul(const std::string &a, const std::string &b) {
    std::string o = "0";
        std::string n;
	for (unsigned i = 0; i < b.length(); i++) {
        n = decimalDigitMul(a, b[i] - '0');
        if (n != "0") {
			for (unsigned j = i + 1; j < b.length(); j++) n += "0";
        }
        o = decimalAdd(o, n);
    }
    return o;
}

//Modexp
std::string decimalModExp(std::string b, std::string e, std::string m) {
    if (e == "0") return "1";
    else if (e == "1") return b;
    else if (decimalMod(e, "2") == "0") {
        std::string o = decimalModExp(b, decimalDiv(e, "2"), m);
        return decimalMod(decimalMul(o, o), m);
    }
    else {
        std::string o = decimalModExp(b, decimalDiv(e, "2"), m);
        return decimalMod(decimalMul(decimalMul(o, o), b), m);
    }
}

// Helper function for exponentiation
std::string _decimalExp(std::string b, std::string e) {
    if (e == "0") return "1";
    else if (e == "1") return b;
    else if (decimalMod(e, "2") == "0") {
        std::string o = _decimalExp(b, decimalDiv(e, "2"));
        return decimalMul(o, o);
    }
    else {
        std::string o = _decimalExp(b, decimalDiv(e, "2"));
        return decimalMul(decimalMul(o, o), b);
    }
}

// Safety wrapper for exponentiation
std::string decimalExp(std::string b, std::string e) {
    if (b != "0" && b != "1" && 
            (e.size() > 5 || b.size() * decimalToUnsigned(e) > 33333)) {
        throw("Exponent way too large: "+e);
    }
    std::string o = _decimalExp(b, e);
    if (o.size() > 10000)
        throw("Out of bounds (maximum 10000 digits)");
    return o;
}

//Is a greater than b? Flag allows equality
bool decimalGt(const std::string &a, const std::string &b, bool eqAllowed) {
    if (a == b) return eqAllowed;
    return (a.length() > b.length()) || (a.length() >= b.length() && a > b);
}

//Remove leading zeros if needed
static std::string removeRedundantLeadingZeros(const std::string &s) {
    int n_zeros = 0;
    for (int i = 0; i < s.size() - 1 && s[i] == '0'; ++i)
        n_zeros++;
    return s.substr(n_zeros);

}

//Subtract the two strings representing decimal values
std::string decimalSub(const std::string &a, const std::string &b) {
    if (b == "0") return a;
    if (b == a) return "0";
    std::string c = prependZeros(b, a.length() - b.length());
	for (unsigned i = 0; i < c.length(); i++) c[i] = '0' + ('9' - c[i]);
    std::string o = decimalAdd(decimalAdd(a, c).substr(1), "1");
    return removeRedundantLeadingZeros(o);
}

//Divide the two strings representing decimal values
std::string decimalDiv(std::string a, std::string b) {
    std::string c = b;
    if (decimalGt(c, a)) return "0";
    int zeroes = -1;
    while (decimalGt(a, c, true)) {
        zeroes += 1;
        c = c + "0";
    }
    c = c.substr(0, c.size() - 1);
    std::string quot = "0";
    while (decimalGt(a, c, true)) {
        a = decimalSub(a, c);
        quot = decimalAdd(quot, "1");
    }
    for (int i = 0; i < zeroes; i++) quot += "0";
    return decimalAdd(quot, decimalDiv(a, b));
}

//Modulo the two strings representing decimal values
std::string decimalMod(const std::string &a, const std::string &b) {
    return decimalSub(a, decimalMul(decimalDiv(a, b), b));
}

//String to int conversion
unsigned decimalToUnsigned(const std::string &a) {
    if (a.size() == 0) return 0;
    else return (a[a.size() - 1] - '0') 
        + decimalToUnsigned(a.substr(0,a.size()-1)) * 10;
}
