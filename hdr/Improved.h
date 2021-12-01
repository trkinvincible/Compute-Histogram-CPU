#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <unordered_map>
#include "../hdr/gzio.h"
#include <fstream>

void temp_function(){


    std::string input_file_name("../res/sample.nrrd");
    std::ifstream input_file_stream(input_file_name, std::ios::in);
    int total_size = 215*215*167;
    if (!input_file_stream.is_open()) {
        std::cout << "failed to open " << input_file_name << '\n';
    }else {
        for (std::string line; std::getline(input_file_stream, line); ) {
            if (line.empty())
                break;
        }
        auto currPosition = input_file_stream.tellg();
        input_file_stream.seekg(0, std::ios_base::end);
        auto end = input_file_stream.tellg();
        auto datasize = end-currPosition;
        std::string str(datasize, '\0');
        std::vector<char> decompressedString;
        input_file_stream.seekg(currPosition);
        if (input_file_stream.read(&str[0], datasize)){

            char* data = str.data();
            boost::iostreams::filtering_ostream decompressingStream;
            decompressingStream.push(boost::iostreams::gzip_decompressor());
            decompressingStream.push(boost::iostreams::back_inserter(decompressedString));
            decompressingStream.write(&str[0], str.size());
            boost::iostreams::close(decompressingStream);
        }

        std::unordered_map<int, int> hist;
        for (int I = 0; I < total_size; I++) {
            auto val = (uint8_t)decompressedString[I];
            hist[val] += 1;
        }
        std::vector<std::pair<int,int>> v;
        for (auto& i : hist){
            v.push_back({i.first, i.second});
        }
        std::sort(v.begin(), v.end());
        for (auto& i : v){
            std::cout /*<< i.first << " " */<< i.second << std::endl;
        }
    }
}
