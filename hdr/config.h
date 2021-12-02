#pragma once

#include <iostream>
#include <chrono>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>

namespace po = boost::program_options;

template <typename DATA>
class config {
    using config_data_t = DATA;
    using add_options_type = std::function<void (config_data_t&, po::options_description&)>;
public:

    config(const add_options_type &add_options):
        add_options(add_options)
    {
        desc.add_options()
                ("help", "produce help")
                ;
    }
    config(const config&) = delete;

    void parse(int argc, char *argv[]) noexcept(false) {
        po::parsed_options parsed = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
        store(parsed, vm);
        notify(vm);

        if (vm.count("help")) {
            std::stringstream ss;
            ss << desc << std::endl;
            throw std::runtime_error(ss.str());
        }

        add_options(config_data, desc);
        store(po::parse_command_line(argc, argv, desc), vm);

        notify(vm);
    }

    template <typename T = std::string>
    auto &get(const char *needle) noexcept(false) {
        try {
            return vm[needle].template as<T>();
        }
        catch(const boost::bad_any_cast &) {
            std::stringstream ss;
            ss << "Get error <" <<  typeid(T).name() << ">(" << needle << ")"  << std::endl;
            throw std::runtime_error(ss.str());
        }
    }
    const config_data_t& data() const {
        return config_data;
    }
    template<typename DATA_TYPE>
    friend std::ostream& operator<<(std::ostream&, const config<DATA_TYPE>&);

private:
    const add_options_type add_options;
    po::variables_map vm;
    po::options_description desc;
    config_data_t config_data;
};

#pragma pack(push, 2)
struct config_data {
    /*
     * Usage: unu histo -b <bins> [-min <value>] [-max <value>] [-t <type>] \
       [-i <nin>] [-o <nout>]

       -b <bins>    = # of bins in histogram (int)
       -min <value> = Value at low end of histogram. Defaults to lowest value found in input nrrd. (double)
       -max <value> = Value at high end of histogram. Defaults to highest value found in input nrrd. (double)
       -t <type>    = type to use for bins in output histogram; default: "uint"
       -i <nin>     = input nrrd
       -o <nout>    = output nrrd (string); default: "-"
    */
    enum OUTPUT_HISTO_BIN_TYPE : uint8_t
    {
        TYPE_UINT_8_T, // unsigned char
        TYPE_UINT_32_T // int
    };
    std::uint16_t bins;
    double min;
    double max;
    uint8_t type;
    std::string input_file_name;
    std::string output_file_name;
};
#pragma pack(pop)
using RkConfig = config<config_data>;

template <typename DATA>
std::ostream& operator<<(std::ostream& s, const config<DATA>& c) {
    for (auto &it : c.vm) {
        s << it.first.c_str() << " ";
        auto& value = it.second.value();
        if (auto v = boost::any_cast<unsigned short>(&value)) {
            s << *v << std::endl;
        }
        else if (auto v = boost::any_cast<unsigned int>(&value)) {
            s << *v << std::endl;
        }
        else if (auto v = boost::any_cast<short>(&value)) {
            s << *v << std::endl;
        }
        else if (auto v = boost::any_cast<long>(&value)) {
            s << *v << std::endl;
        }
        else if (auto v = boost::any_cast<int>(&value)) {
            s << *v << std::endl;
        }
        else if (auto v = boost::any_cast<std::string>(&value)) {
            s << *v << std::endl;
        }
        else if (auto v = boost::any_cast<config_data::OUTPUT_HISTO_BIN_TYPE>(&value)) {
            s << *v << std::endl;
        }
        else {
            throw boost::program_options::validation_error ("Invalid Argument");
        }
    }
    return s;
}
