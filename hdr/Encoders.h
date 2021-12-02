#pragma once

#include <array>
#include <memory>
#include <map>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#ifdef MEMORY_OPTIMIZED
#include "../hdr/gzio.h"
#endif
#include "../hdr/command.h"

class ComputeHistogram;

namespace RkEncoders {

enum EncoderType : int16_t {
    EncodingTypeUnknown = -1,
    EncodingTypeGzip = 0,
    EncodingTypeRaw,
    EncodingTypeAscii,
    EncodingTypeHex,
    EncodingTypeBzip2,
    EncodingTypeZRL,
    EncodingTypeLast
};

class IEncoder {
public:
    virtual bool Parse(std::ifstream& file_stream, const std::string& file_name,
                                           std::size_t data_size, std::vector<std::string>& fill) const = 0;
    friend class ComputeHistogram;
};

class GzipEncoder : public IEncoder{

public:
    inline bool Parse(std::ifstream& input_file_stream, const std::string& file_name,
                                   std::size_t data_size, std::vector<std::string>& fill) const override{

        static std::string empty_string("");

#ifdef MEMORY_OPTIMIZED
        // read compressed data in chunks with native zlib APIs
        FILE* file = fopen(file_name.data(), "rb");
        std::size_t len = 0;
        // go until data part
        for (char* line = NULL; (getline(&line, &len, file)) != -1; ) {
            if (std::isspace(line[0])){
                break;
            }
        }
        gzFile gzfin;
        if ((gzfin = GzOpen(file, "rb")) == Z_NULL) {
            return {empty_string};
        }
        char* data = new char[data_size + 1]();
        unsigned int didread, sizeChunk{data_size / Task::NO_OF_CORES};
        std::size_t sizeRed{0};
        int error;
        char* copy = data;
        while (!(error = GzRead(gzfin, copy, sizeChunk, &didread))
               && didread > 0) {
            /* Increment the data pointer to the next available chunk. */
            copy += didread;
            {
                std::string tmp(data + sizeRed, didread);
                fill.push_back(std::move(tmp));
            }
            sizeRed += didread;
            if (data_size >= sizeRed && data_size - sizeRed < sizeChunk){
                sizeChunk = (uint)(data_size - sizeRed);
            }
        }
        GzClose(gzfin);
        fclose(file);

        return true;
#else
        auto start = input_file_stream.tellg();
        input_file_stream.seekg(0, std::ios_base::end);
        auto end = input_file_stream.tellg();
        auto datasize = (end - start);
        input_file_stream.seekg(start);

        std::string str(datasize, '\0');
        if (input_file_stream.read(&str[0], datasize)){
            try{
                fill.resize(1);
                // Decompress whole string as possibility of corrupted data.
                boost::iostreams::filtering_ostream decompressingStream;
                decompressingStream.push(boost::iostreams::gzip_decompressor());
                decompressingStream.push(boost::iostreams::back_inserter(fill[0]));
                decompressingStream.write(&str[0], str.size());
                boost::iostreams::close(decompressingStream);
                return true;
            }catch(std::exception e){
                std::cout << e.what();
                return false;
            }
        }
#endif
        return true;
    }

};

class RawEncoder : public IEncoder{
public:
    bool Parse(std::ifstream& file_stream, const std::string& file_name,
                                   std::size_t data_size, std::vector<std::string>& fill) const override{
        return false;
    }
};

std::array<std::shared_ptr<IEncoder>, 2> EncodersClasses = {
    std::make_shared<GzipEncoder>(),
    std::make_shared<RawEncoder>(),
    //.. same order as enum
};

std::map<std::string, std::shared_ptr<IEncoder>> Encoders = {
    {"GZIP", EncodersClasses[EncoderType::EncodingTypeGzip]},
    {"RAW", EncodersClasses[EncoderType::EncodingTypeRaw]},
};
}
