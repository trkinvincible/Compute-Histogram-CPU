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

static std::map<std::string, PAYLOAD_TYPE> PayLoadType = {
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
        if (s.empty())
            continue;
        boost::algorithm::trim(s);
        result.push_back(s);
    }

    return result;
}
std::size_t NearestPowerOfTwo(std::size_t v){

    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;

    v++;

    return v;
}


// Lock-free is not wait free. with this container no need memory fence.
// will yield better performance in GPU/Metal
template<class T = std::uint32_t>
class AlignedContinuousMemory
{
    static constexpr std::size_t  CACHELINE_SIZE{64};

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

    AlignedContinuousMemory(std::size_t size = 0)
        : m_Size(size),
          m_Data(nullptr)
    {
        if (m_Size > 0){
            try{
                const auto s = NearestPowerOfTwo(m_Size);
                m_Data = static_cast<T*>(std::aligned_alloc(std::max(CACHELINE_SIZE, s), sizeof(T) * size));
                memset(m_Data, 0, sizeof(T) * size);
            }catch(std::exception& ex){
                std::cerr << " histo bins memory allocation failed" << std::endl;
                throw;
            }
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
    if (len < 8)
        return std::accumulate(beg, end, 1, std::multiplies<std::size_t>());

    RandomIt mid = beg + len/2;
    auto handle = std::async(std::launch::async,
                             parallel_multiply<RandomIt>, mid, end);
    int product = parallel_multiply(beg, mid);
    return product + handle.get();
}

// ref: http://teem.sourceforge.net/nrrd/format.html#encoding
template<typename T>
constexpr bool isValidNrrdEncodedType(){
    return std::is_integral_v<T> ||
           std::is_floating_point_v<T>;
}

template<typename T, typename = std::enable_if_t<isValidNrrdEncodedType<T>()>>
T DecodeBytesSpcialized(const std::string_view& data, const std::size_t index){
    T value;
    std::memcpy(&value, (data.data() + index), sizeof(T));
    return value;
}

template <>
std::uint8_t DecodeBytesSpcialized<std::uint8_t>(const std::string_view& data, std::size_t index){
    // static cast is faster
    return static_cast<std::uint8_t>(data[index]);
}

template <typename T>
constexpr inline const T& Clamp(const T& min, const T& val, const T& max){
    return std::max(min, std::min(max, val));
}

}
