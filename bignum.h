#ifndef ETHSERP_BIGNUM
#define ETHSERP_BIGNUM

const std::string nums = "0123456789";

std::string intToDecimal(int branch);

std::string decimalAdd(std::string a, std::string b);

std::string decimalMul(std::string a, std::string b);

std::string decimalSub(std::string a, std::string b);

std::string decimalDiv(std::string a, std::string b);

std::string decimalMod(std::string a, std::string b);

int decimalToInt(std::string a);

#endif
