#pragma once

#include <string>
#include <vector>
#include <map>
#include <boost/algorithm/string/trim.hpp>

namespace RkUtil {

enum class PAYLOAD_TYPE
{
    TypeUChar = 0,
    TypeInt
};

const std::size_t PAYLOAD_TYPE_SIZE[] = {
  1,
  4,
};

std::map<std::string, PAYLOAD_TYPE> PayLoadType = {
    {"UNSIGNED CHAR", PAYLOAD_TYPE::TypeUChar},
    {"INT", PAYLOAD_TYPE::TypeInt}
};

std::string str_toupper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return s;
}

std::vector<std::string> Split(const std::string& input, const char delimiter){

    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string s;
    while (std::getline(ss, s, delimiter)) {
        boost::algorithm::trim(s);
        result.push_back(s);
    }

    return result;
}
}
