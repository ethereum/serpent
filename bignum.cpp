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

//Is a greater than b? Flag allows equality
bool decimalGt(const std::string &a, const std::string &b, bool eqAllowed) {
    if (a == b) return eqAllowed;
    return (a.length() > b.length()) || (a.length() >= b.length() && a > b);
}

//Remove leading zeros if needed
static std::string removeRedundantLeadingZeros(const std::string &s) {
    int n_zeros = 0;
    for (size_t i = 0; i < s.size() - 1 && s[i] == '0'; ++i)
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
