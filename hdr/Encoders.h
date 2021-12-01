#pragma once

#include <array>
#include <memory>
#include <map>

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
};

class GzipEncoder : public IEncoder{
public:
};

class RawEncoder : public IEncoder{
public:
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
