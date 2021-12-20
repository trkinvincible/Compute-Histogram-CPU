

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <boost/algorithm/string/trim.hpp>

namespace RkUtil {

const int MAX_HIST_BIN_SIZE = 300;

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
// this is static vector == array so no one past iterator for end. write after defined size will be buffer overflow
template<typename T, std::size_t N = MAX_HIST_BIN_SIZE>
class AlignedContinuousMemory
{
    static constexpr std::size_t  CACHELINE_SIZE{64};
    typename std::aligned_storage<sizeof(T), alignof(T)>::type data[N];

public:
    AlignedContinuousMemory(std::size_t size = 0)
        : m_Data(nullptr),
          m_Size(0),
          m_CurrPos(0)
    {
        if constexpr (N <= MAX_HIST_BIN_SIZE){
            m_Data = reinterpret_cast<T*>(&data);
            m_Size = N;
        }else{
            if (size > 0){
                try{
                    const auto s = NearestPowerOfTwo(size);
                    m_Data = static_cast<T*>(std::aligned_alloc(std::max(CACHELINE_SIZE, s), sizeof(T) * size));
                    clear();
                }catch(std::exception& ex){
                    std::cerr << " histo bins memory allocation failed" << std::endl;
                    throw;
                }
                m_Size = size;
            }
        }
    }

    AlignedContinuousMemory(const AlignedContinuousMemory& other){

        AlignedContinuousMemory(other.m_Size);
        std::uninitialized_copy(other.m_Data, other.m_Size, this->m_Data);
    }

    AlignedContinuousMemory(AlignedContinuousMemory&& other) noexcept{

        this->m_Data = std::exchange(other.m_Data, nullptr);
        this->m_Size = std::exchange(other.m_Size, 0);
    }

    AlignedContinuousMemory& operator=(AlignedContinuousMemory& other){

        if (this != &other){
            // If exception occurs in mem alloc exit here keeping the "other" untouched.
            AlignedContinuousMemory m(other.m_Size);

            if (this->m_Data){
                std::free(m_Data);
                m_Size = 0;
            }
            this->m_Data = m.m_Data;
            this->m_Size = m.m_Size;
            std::uninitialized_copy_n(other.m_Data, other.m_Size, this->m_Data);
            m.m_Data = nullptr;
            m.m_Size = 0;
        }

        return *this;
    }

    AlignedContinuousMemory& operator=(AlignedContinuousMemory&& other) noexcept{

        if (this != &other){

            if (this->m_Data){
                std::free(m_Data);
                m_Size = 0;
            }
            std::swap(other.m_Data, this->m_Data);
            std::swap(other.m_Size, this->m_Size);
        }

        return *this;
    }

    template<typename ...Args>
    void emplace_back(Args&&... args) {

        if (m_CurrPos == m_Size + 1){
            assert(false && "out of bound access");
        }
        new(&m_Data[m_CurrPos++]) T(std::forward<Args>(args)...);
    }

    T& operator[](std::size_t pos) {

        if (pos <= m_Size){
            return *reinterpret_cast<T*>(&m_Data[pos]);
        }else{
            assert(false && "out of bound access");
        }
    }

    ~AlignedContinuousMemory() {

        for(std::size_t pos = 0; pos < m_Size; ++pos) {
            reinterpret_cast<T*>(&m_Data[pos])->~T();
        }
        if (m_Data && !isInStack()){
            std::free(m_Data);
            m_Data = nullptr;
        }
    }

    inline bool isInStack(){

        return (m_Size <= MAX_HIST_BIN_SIZE);
    }

    std::size_t size() const {

        return m_Size;
    }

    void clear() {

        if (m_Data){
            memset(m_Data, 0, sizeof(T) * m_Size);
        }
    }

    T* start() { return m_Data; }
    T* end() { return m_Data + m_Size; }

private:
    T* m_Data;
    std::size_t m_Size;
    std::size_t m_CurrPos;
};

template<typename T, std::size_t N>
class BinMemPool : public std::enable_shared_from_this<BinMemPool<T, N>>{

public:
    std::shared_ptr<BinMemPool<T, N>> getBinMemPool() {
        return this->shared_from_this();
    }

    ~BinMemPool(){

        assert(m_AcquiredBuffers.empty());
        for (auto& i : m_AvailableBuffers){
            delete i;
        }
    }

    AlignedContinuousMemory<T, N>* GetBuffer(){

        std::lock_guard<std::mutex> lk(m_Gurad);

        if (m_AvailableBuffers.empty()){
            AlignedContinuousMemory<T, N>* m = new AlignedContinuousMemory<T, N>(N);
            m_AcquiredBuffers.insert(m);
            return m;
        }else{
            auto itr = m_AvailableBuffers.begin();
            auto d = *itr;
            m_AcquiredBuffers.insert(d);
            m_AvailableBuffers.erase(itr);
            d->clear();
            return d;
        }
    }

    void ReleaseBuffer(AlignedContinuousMemory<T, N>* freeBuf){

        std::lock_guard<std::mutex> lk(m_Gurad);

        auto itr = m_AcquiredBuffers.find(freeBuf);
        assert(itr != m_AcquiredBuffers.end());
        m_AvailableBuffers.insert(*itr);
        m_AcquiredBuffers.erase(itr);
    }

private:
    std::mutex m_Gurad;
    std::unordered_set<AlignedContinuousMemory<T, N>*> m_AvailableBuffers;
    std::unordered_set<AlignedContinuousMemory<T, N>*> m_AcquiredBuffers;
};

// Class for memory recycling. RAII pattern
template<typename T, std::size_t N = MAX_HIST_BIN_SIZE>
struct ScopedStaticVector{

    using value_type = RkUtil::AlignedContinuousMemory<T, N>;
    using iterator = T*;

public:
    ScopedStaticVector(std::size_t size = 0)
        : m_Data(nullptr),
          m_Size(size){

        m_Data = m_MemPool->getBinMemPool()->GetBuffer();
    }

    ~ScopedStaticVector(){

        if (m_Data && m_CanRelease){
            m_MemPool->getBinMemPool()->ReleaseBuffer(m_Data);
        }
    }

    ScopedStaticVector(const ScopedStaticVector& rhs)=default;

    ScopedStaticVector(ScopedStaticVector&& rhs) noexcept{

        this->m_Data = std::exchange(rhs.m_Data, nullptr);
        this->m_CanRelease = std::exchange(rhs.m_CanRelease, true);
    }

    ScopedStaticVector& operator=(ScopedStaticVector&& rhs){

        if (this != &rhs){
            if (m_Data){
                m_MemPool->getBinMemPool()->ReleaseBuffer(m_Data);
                m_Data = nullptr;
            }
            this->m_Data = std::exchange(rhs.m_Data, nullptr);
            this->m_CanRelease = std::exchange(rhs.m_CanRelease, true);
        }

        return *this;
    }

    void canRelease(const bool b) { m_CanRelease = b; }

    RkUtil::AlignedContinuousMemory<T, N>* operator->(){ return m_Data; }

    T& operator[](const std::size_t index) const { return (*m_Data)[index]; }

    iterator begin() const{ return iterator(m_Data->start()); }

    iterator end() const { return iterator(m_Data->end()); }

private:
    // Always armed to release the memory unless explicity set
    bool m_CanRelease = true;
    value_type* m_Data;
    std::size_t m_Size;
    // bin will have multiple singleton instances of mempool of different sizes. MAX_HIST_BIN_SIZE is TODO:
    static std::shared_ptr<BinMemPool<T, N>> m_MemPool;
};

template <typename T, std::size_t N>
std::shared_ptr<BinMemPool<T, N>> ScopedStaticVector<T, N>::m_MemPool = std::make_shared<BinMemPool<T, N>>();

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
