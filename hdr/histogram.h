#pragma once

#include <fstream>
#include <iostream>
#include <array>
#include <vector>
#include <thread>
#include <numeric>
#include <future>
#include <string_view>
#include <deque>

#include <boost/interprocess/file_mapping.hpp>

#include "../hdr/command.h"
#include "../hdr/config.h"
#include "../hdr/Encoders.h"
#include "../hdr/Utility.h"

class ComputeHistogram : public Task{

    // Must use some factory method to generate this varaible for type and size based on config
    using bins_type = RkUtil::ScopedStaticVector<std::uint32_t, 300>;

public:
    explicit ComputeHistogram(const std::unique_ptr<RkConfig>& config)
        : m_Config(config) {

        // exit if cannot operate. RAII.
        if (m_Config->data().bins < m_Config->data().max){
            throw std::runtime_error("bins must be >= max value to represent");
        }
        m_Bins = m_Config->data().bins;
    }

    bool ParseInput() override{

        const std::string& input_file_name = m_Config->data().input_file_name;

        std::ifstream input_file_stream(input_file_name, std::ios::in);
        if (!input_file_stream.is_open()){
            std::cout << __FUNCTION__ << "input_file: " <<
                         input_file_name << " not found" << std::endl;
            return false;
        }

        for (std::string line; std::getline(input_file_stream, line); ) {
            // strictly NRRD file will have a empty line to seperate header and data. validation must be done on other elemnst as well
            if (line.empty())
                break;

            std::vector<std::string> vals = RkUtil::Split(line, ':');
            if (vals.size() != 2)
                continue;

            if (RkUtil::str_toupper(vals[0]) == "TYPE"){
                m_Type = RkUtil::PayLoadType[RkUtil::str_toupper(vals[1])];
                continue;
            }

            if (RkUtil::str_toupper(vals[0]) == "DIMENSION"){
                try {
                    m_Dimension = std::stoi(vals[1]);
                }catch (...) {
                    std::cerr << "Invalid dimensions: " << vals[1] << std::endl;
                    m_Dimension = 0;
                };
                continue;
            }

            if (RkUtil::str_toupper(vals[0]) == "SIZES"){
                // No need const & as copy elision will happen.
                std::vector<std::string> s = RkUtil::Split(vals[1], ' ');
                s.resize(MAX_DIMENSIONS, "1");
                std::generate(m_Sizes.begin(), m_Sizes.end(), [n = 0, s]() mutable{
                    std::size_t v = 0;
                    try{
                        v = std::stoi(s[n++]);
                    }catch(...){
                        v = 1;
                    }
                    return v;
                });

                continue;
            }

            if (RkUtil::str_toupper(vals[0]) == "ENCODING"){
                auto itr = RkEncoders::Encoders.find(RkUtil::str_toupper(vals[1]));
                if (itr == RkEncoders::Encoders.end()){
                    std::cerr << "Invalid encoding not (yet) supported" << std::endl;
                    return false;
                }
                m_Encoder = itr->second;
                continue;
            }

        }

        // Since product obtained might be huge lets parallelize.
        m_DataSize = RkUtil::parallel_multiply(m_Sizes.begin(), m_Sizes.end());

        if (m_DataSize <= 0){
            std::cout << "Empty nrrd data file (not header file) so bail out" << std::endl;
            return false;
        }

        if (!m_Encoder->Parse(input_file_stream, input_file_name,
                              (m_DataSize * RkUtil::PAYLOAD_TYPE_SIZE[(int)m_Type]), m_DecompressedData)){
            return false;
        }

        // return true only if input data is fully validated
        return true;
    }

    bool Operate() override{

        try{
            /*
             * Not good idea to Operate() while Parse() in action because
             * # if mid data is corrupted Operate() on previous data goes stale
             * Not a good idea to read the file with multiple threads as disk reading HW needle
             * is still one which might have to jump sectors and worsen performance.
             * or memory map the whole file will exhaust memory if file is too large (can shrink though).
            */
            for (std::string& slice : m_DecompressedData){
                std::size_t individual_buffer_size = (slice.size() / NO_OF_CORES);
                boost::interprocess::offset_t offset = 0;
                const std::size_t datasize = slice.size();
                for(uint i = 1; i <= NO_OF_CORES; i++, offset += individual_buffer_size){
                    if(i == NO_OF_CORES){
                        individual_buffer_size = datasize - ((i-1) * individual_buffer_size);
                    }

                    const char* start = (slice.data() + offset);
                    std::string_view tmp(start, individual_buffer_size);
                    const auto min = m_Config->data().min;
                    const auto max = m_Config->data().max;
                    auto fu = std::async(std::launch::async, [min, max, this, data = std::move(tmp)]() mutable{

                        bins_type hist(m_Bins);
                        for (std::size_t idx = 0; idx < data.size(); idx += RkUtil::PAYLOAD_TYPE_SIZE[(int)m_Type]) {
                            auto val = DecodeBytes(data, idx);
                            // TODO: caution!!.
                            // strictly validate for extents because hist[] is pre allocated based on bins.
                            // must implement minmax
                            val = RkUtil::Clamp(min, val, max);
                            hist[val] += 1;
                        }

                        hist.canRelease(false);
                        return hist;
                    });

                    m_Futures.push_back(std::move(fu));
                }
            }
        }catch(std::exception& ex){
            std::cout << "exception occured while computing histogram: why?: " << ex.what() << std::endl;
            return false;
        }

        return true;
    }

    void WriteOutput() {

        /*
         * DESIGN NOTE:
         * As this exercise is latency sensitive than memory sensitive.
         * threads work on independent memory. sharing the same memory and using atomics or mutex will slow down.
         * atomics is lock free but not wait free.
         * Eg:
         *  |   |   |   |
         *  |   |   |   |
         *  |   |   |   |
         *  |   |   |   |
         *  |   |   |   |
         *  |   |   |   |
         *  v1  v2  v3  v4
         *  v0 : v0 += v1 && v0 += v2 && v0 += v3 && v0 += v4 need syncronization and slow
         *  instead
         *  v00 : v1 + v2 && v01 : v3 + v4
         *  v0 : v00 + v01 no sync needed.
         *  * (&&) above mean multithreading.
         *
         * If GPU available vectorization (tensors) with a ton of threads litreally one (grids/blocks)thread per
         * pixel shall be spawned with CUDA/OpenCL/Metal APIs or with SIMD instructions(Single instruction, multiple data )
         *
         * Maybe I will submit another solution in '*.cu' with only the kernel function later.
        */

        bins_type ret;
        while(!m_Futures.empty()){
            auto& fu1 = m_Futures.front();
            ret = fu1.get();
            m_Futures.pop_front();

            if (m_Futures.empty())
                break;

            std::promise<bins_type> merge_promise;
            std::future<bins_type> merge_future = merge_promise.get_future();
            auto mergefu = std::async(std::launch::async, [this, first = std::move(ret), second = std::move(merge_future)]() mutable{
                auto other = second.get();
                bins_type ret(m_Bins);
                assert(first->size() == other->size());
                for (std::size_t i = 0; i < ret->size(); ++i){
                    ret[i] = first[i] + other[i];
                }
                first.canRelease(true);
                other.canRelease(true);

                ret.canRelease(false);
                return ret;
            });

            m_Futures.push_back(std::move(mergefu));
            auto& fu2 = m_Futures.front();
            merge_promise.set_value(fu2.get());
            m_Futures.pop_front();
        }

        // Copy the output for unit test
        std::ofstream output(m_Config->data().output_file_name);
        const auto s = ret->size();
        m_Output.resize(s);
        for (std::size_t i = 0; i < s; ++i){
            const auto& c = ret[i];
            m_Output[i] = c;
            output << "(" << i << ", " << c << ")" << '\n';
        }

        ret.canRelease(true);

        output.close();
#if 0
        const std::string tmp = std::string("subl ") + std::string(m_Config->data().output_file_name);
        system(tmp.data());
#endif
    }

#ifdef RUN_CATCH
    std::size_t OutputVal(){

        return std::accumulate(m_Output.begin(), m_Output.end(), 0);
    }
#endif

private:

    double DecodeBytes(const std::string_view& data, std::size_t index){

        switch (RkUtil::PAYLOAD_TYPE_SIZE[(int)m_Type]) {

        case 1:         //uchar
            return RkUtil::DecodeBytesSpcialized<uint8_t>(data, index);
            break;
        case 2:         // short
            return RkUtil::DecodeBytesSpcialized<int16_t>(data, index);
            break;
        }

        return 0.0;
    }

    static constexpr int MAX_DIMENSIONS = 16;

    RkUtil::PAYLOAD_TYPE m_Type;
    std::uint16_t m_Bins;
    std::uint8_t m_Dimension;
    std::array<std::size_t, MAX_DIMENSIONS> m_Sizes;
    std::shared_ptr<RkEncoders::IEncoder> m_Encoder;
    std::vector<std::uint32_t> m_Output;
    std::vector<std::string> m_DecompressedData;
    std::deque<std::future<bins_type>> m_Futures;
    std::size_t m_DataSize;
    const std::unique_ptr<RkConfig>& m_Config;
};
