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
  sizeof(char),
  sizeof(uint),
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

// Lock-free is not wait free. with this memory no need memory fence
template<class T = std::uint32_t>
class AlignedContinuousMemory
{
    static constexpr int  CACHELINE_SIZE{64};

public:
    AlignedContinuousMemory(AlignedContinuousMemory&& other) = default;
    AlignedContinuousMemory& operator=(AlignedContinuousMemory&& other) = default;

    AlignedContinuousMemory(std::size_t size)
        : m_Size(size){
        if (m_Size > 0)
            m_Data = static_cast<T*>(std::aligned_alloc(CACHELINE_SIZE, sizeof(T) * size));
    }

    template<typename ...Args>
    void emplace_back(Args&&... args) {
        new(&m_Data[m_Size]) T(std::forward<Args>(args)...);
        ++m_Size;
    }

    T& operator[](std::size_t pos) {
        return *reinterpret_cast<T*>(&m_Data[pos]);
    }

    ~AlignedContinuousMemory() {
        for(std::size_t pos = 0; pos < m_Size; ++pos) {
            reinterpret_cast<T*>(&m_Data[pos])->~T();
        }
    }

    std::size_t size() const {
        return m_Size;
    }

private:
    std::size_t m_Size = 0;
    T* m_Data;
};

template <typename RandomIt>
int parallel_multiply(RandomIt beg, RandomIt end)
{
    auto len = end - beg;
    if (len < 10000)
        return std::accumulate(beg, end, 1, std::multiplies<std::size_t>());

    RandomIt mid = beg + len/2;
    auto handle = std::async(std::launch::async,
                             parallel_multiply<RandomIt>, mid, end);
    int product = parallel_multiply(beg, mid);
    return product + handle.get();
}

}
