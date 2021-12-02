#pragma once

#include <string>
#include <vector>
#include <map>
#include <boost/algorithm/string/trim.hpp>

namespace RkUtil {

enum class PAYLOAD_TYPE {
    TypeUChar = 0,
    TypeShort
};

const std::size_t PAYLOAD_TYPE_SIZE[] = {
  sizeof(char),
  sizeof(short),
};

std::map<std::string, PAYLOAD_TYPE> PayLoadType = {
    {"UNSIGNED CHAR", PAYLOAD_TYPE::TypeUChar},
    {"SHORT", PAYLOAD_TYPE::TypeShort}
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
    AlignedContinuousMemory(const AlignedContinuousMemory& other){

        AlignedContinuousMemory(other.m_Size);
        std::uninitialized_copy(other.m_Data, other.m_Size, this->m_Data);
    }

    AlignedContinuousMemory(AlignedContinuousMemory&& other){
        this->m_Data = std::exchange(other.m_Data, nullptr);
        this->m_Size = std::exchange(other.m_Size, 0);
    }

    AlignedContinuousMemory& operator=(AlignedContinuousMemory& other){
        this->m_Data = other.m_Data;
        this->m_Size = other.m_Size;

        return *this;
    }

    AlignedContinuousMemory& operator=(AlignedContinuousMemory&& other){
        if (this != &other){

            if (this->m_Data){
                std::free(m_Data);
                m_Size = 0;
            }
            AlignedContinuousMemory m(other.m_Size);
            *this = m;
            std::uninitialized_move_n(other.m_Data, other.m_Size, this->m_Data);
            m.m_Data = nullptr;
            m.m_Size = 0;
        }
        return *this;
    }

    AlignedContinuousMemory(std::size_t size)
        : m_Size(size),
          m_Data(nullptr)
    {
        if (m_Size > 0){
            m_Data = static_cast<T*>(std::aligned_alloc(CACHELINE_SIZE, sizeof(T) * size));
            memset(m_Data, 0, sizeof(T) * size);
        }
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
        if (m_Data){
            std::free(m_Data);
            m_Data = nullptr;
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
