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
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "../hdr/command.h"
#include "../hdr/config.h"
#include "../hdr/Encoders.h"
#include "../hdr/Utility.h"

template <typename T>
T extract(const std::string_view& data, std::size_t index){
    return (T)(data[index]);
}

class ComputeHistogram : public Task{

public:
    explicit ComputeHistogram(const std::unique_ptr<RkConfig>& config)
        : m_Config(config),NO_OF_CORES(std::thread::hardware_concurrency()) {}

    bool ParseInput() override{

        std::ifstream input_file_stream(m_Config->data().input_file_name.data(), std::ios::in);
        if (!input_file_stream.is_open()){
            std::cout << __FUNCTION__ << "input_file_name not found" << std::endl;
            return false;
        }

        for (std::string line; std::getline(input_file_stream, line); ) {

            // strictly NRRD file will have a empty line to seperate header and data
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
                    m_Dimension = 0;
                };
                continue;
            }

            if (RkUtil::str_toupper(vals[0]) == "SIZES"){

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
                m_Encoder = RkEncoders::Encoders[RkUtil::str_toupper(vals[1])];
                continue;
            }

        }

        auto start = input_file_stream.tellg();
        input_file_stream.seekg(0, std::ios_base::end);
        auto end = input_file_stream.tellg();
        auto datasize = (end - start);
        input_file_stream.seekg(start);

        std::string str(datasize, '\0');
        if (input_file_stream.read(&str[0], datasize)){

            try{
                // Decompress whole string as possibility of corrupted data.
                boost::iostreams::filtering_ostream decompressingStream;
                decompressingStream.push(boost::iostreams::gzip_decompressor());
                decompressingStream.push(boost::iostreams::back_inserter(m_DecompressedData));
                decompressingStream.write(&str[0], str.size());
                boost::iostreams::close(decompressingStream);
            }catch(std::exception e){
                m_DecompressedData.clear();
                std::cout << e.what();
                return false;
            }
        }else{
            return false;
        }

        // return true only if input data is fully validated
        return true;
    }

    bool Operate() override{

        std::size_t individual_buffer_size = (m_DecompressedData.size() / NO_OF_CORES);
        boost::interprocess::offset_t offset = 0;
        const std::size_t datasize = m_DecompressedData.size();
        for(uint i = 1; i <= NO_OF_CORES; i++, offset += individual_buffer_size){

            if(i == NO_OF_CORES){
                individual_buffer_size = datasize - ((i-1) * individual_buffer_size);
            }
            char* start = (m_DecompressedData.data() + offset);
            std::string_view tmp(start, individual_buffer_size);
            const std::size_t total_pixel_count = std::accumulate(m_Sizes.begin(), m_Sizes.end(), 1, std::multiplies<int>());
            auto fu = std::async(std::launch::async, [this, data = std::move(tmp)]() mutable{

                std::vector<std::uint32_t> hist(m_Config->data().bins, 0);
                for (int idx = 0; idx < data.size(); idx += RkUtil::PAYLOAD_TYPE_SIZE[(int)m_Type]) {
                    auto val = extract<uint8_t>(data, idx);
                    if ( val < m_Config->data().min || val > m_Config->data().max)
                        continue;
                    hist[val] += 1;
                }
                return hist;
            });

            m_Futures.push_back(std::move(fu));
        }

        return true;
    }

    void WriteOutput() {

        std::vector<uint32_t> ret;
        while(!m_Futures.empty()){
            auto& fu1 = m_Futures.front();
            ret = fu1.get();
            m_Futures.pop_front();

            if (m_Futures.empty())
                break;

            std::promise<std::vector<uint32_t>> merge_promise;
            std::future<std::vector<uint32_t>> merge_future = merge_promise.get_future();
            auto mergefu = std::async(std::launch::async, [this, first = std::move(ret), second = std::move(merge_future)]() mutable{

                std::vector<std::uint32_t> ret(256, 0);
                auto other = second.get();
                for (int i = 0; i < 256; ++i){
                    ret[i] = first[i] + other[i];
                }
                return ret;
            });
            m_Futures.push_back(std::move(mergefu));
            auto& fu2 = m_Futures.front();
            merge_promise.set_value(fu2.get());
            m_Futures.pop_front();
        }

        std::ofstream output(m_Config->data().output_file_name);
        for (int i = 0; i < ret.size(); ++i){
            output << "(" << i << ", " << ret[i] << ")" << '\n';
        }
        const std::string tmp = std::string("subl ") + std::string(m_Config->data().output_file_name);
        system(tmp.data());
    }

private:
    static constexpr int MAX_DIMENSIONS = 16;
    const std::size_t NO_OF_CORES;

    RkUtil::PAYLOAD_TYPE m_Type;
    std::uint8_t m_Dimension;
    std::array<std::size_t, MAX_DIMENSIONS> m_Sizes;
    std::shared_ptr<RkEncoders::IEncoder> m_Encoder;

    std::string m_DecompressedData;
    std::deque<std::future<std::vector<uint32_t>>> m_Futures;
    const std::unique_ptr<RkConfig>& m_Config;
};
