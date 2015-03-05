#ifndef ETHSERP_BIGNUM
#define ETHSERP_BIGNUM

const std::string nums = "0123456789";

const std::string tt256 = 
"115792089237316195423570985008687907853269984665640564039457584007913129639936"
;

const std::string tt255 =
"57896044618658097711785492504343953926634992332820282019728792003956564819968"
;

std::string unsignedToDecimal(unsigned branch);

std::string decimalAdd(const std::string &a, const std::string &b);

std::string decimalMul(const std::string &a, const std::string &b);

std::string decimalSub(const std::string &a, const std::string &b);

std::string decimalDiv(std::string a, std::string b);

std::string decimalMod(const std::string &a, const std::string &b);

bool decimalGt(const std::string &a, const std::string &b, bool eqAllowed=false);

unsigned decimalToUnsigned(const std::string &a);

#endif
