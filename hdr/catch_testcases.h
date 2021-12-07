#pragma once

// validate output
#define CATCH_CONFIG_MAIN
#include "../hdr/catch.hpp"

std::unique_ptr<Task> Test_Function(std::string filename)
{
    std::unique_ptr<RkConfig> config = std::make_unique<RkConfig>([filename](config_data &d, boost::program_options::options_description &desc){
        desc.add_options()
                ("-b", boost::program_options::value<std::uint16_t>(&d.bins)->default_value(300), "# of bins in histogram (int)")
                ("-min", boost::program_options::value<double>(&d.min)->default_value(0.0), "Value at low end of histogram. Defaults to lowest value found in input nrrd. (double)")
                ("-max", boost::program_options::value<double>(&d.max)->default_value(300.0), "Value at high end of histogram. Defaults to highest value found in input nrrd. (double)")
                ("-t", boost::program_options::value<uint8_t>(&d.type)->default_value(1), "type to use for bins in output histogram; default: \"uint\"")
                ("-i", boost::program_options::value<std::string>(&d.input_file_name)->default_value(filename.data()), "input nrrd")
                ("-o", boost::program_options::value<std::string>(&d.output_file_name)->default_value("../solution.txt"), "solution file");
    });

    try {
        char* a[] = {"Ex2", "-i", filename.data()};
        config->parse(3, a);
    }
    catch(std::exception const& e) {
        std::cout << e.what();
    }

    std::unique_ptr<Task> task = std::make_unique<ComputeHistogram>(config);
    task->Compute();

    return task;
}

TEST_CASE("Validate Output with Total Pixels")
{
    // check /res/short-gzip.nrrd
    const auto& t1 = Test_Function("../res/short-gzip.nrrd");
    // check if solution file generated
    std::ifstream solution_file("../solution.txt", std::ios::in);
    if (!solution_file.is_open())
        REQUIRE(false);
    auto o1 = (256*256*130);
    REQUIRE(t1->OutputVal() == o1);

    // check /res/uchar-gzip.nrrd
    const auto& t2 = Test_Function("../res/uchar-gzip.nrrd");
    auto o2 = (215*215*167);
    REQUIRE(t2->OutputVal() == o2);

    // check /res/uchar-raw.nrrd
    const auto& t3 = Test_Function("../res/uchar-raw.nrrd");
    auto o3 = (3*128*128);
    REQUIRE(t3->OutputVal() == o3);
}
