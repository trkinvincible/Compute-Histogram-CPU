#include "../hdr/histogram.h"
#include "../hdr/config.h"

std::size_t const Task::NO_OF_CORES = std::thread::hardware_concurrency();

#ifndef RUN_CATCH
int main(int argc, char *argv[])
{
    std::unique_ptr<RkConfig> config = std::make_unique<RkConfig>([](config_data &d, boost::program_options::options_description &desc){
        desc.add_options()
                ("-b", boost::program_options::value<std::uint16_t>(&d.bins)->default_value(300), "# of bins in histogram (int)")
                ("-min", boost::program_options::value<double>(&d.min)->default_value(0.0), "Value at low end of histogram. Defaults to lowest value found in input nrrd. (double)")
                ("-max", boost::program_options::value<double>(&d.max)->default_value(300.0), "Value at high end of histogram. Defaults to highest value found in input nrrd. (double)")
                ("-t", boost::program_options::value<uint8_t>(&d.type)->default_value(1), "type to use for bins in output histogram; default: \"uint\"")
                ("-i", boost::program_options::value<std::string>(&d.input_file_name)->default_value("../res/sample.nrrd"), "input nrrd")
                ("-o", boost::program_options::value<std::string>(&d.output_file_name)->default_value("../solution.txt"), "solution file");
    });

    try {

        config->parse(argc, argv);
    }
    catch(std::exception const& e) {

        std::cout << e.what();
        return 0;
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::unique_ptr<Task> task = std::make_unique<ComputeHistogram>(config);
    task->Compute();

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    std::cout << "total runtime : " << diff.count() << " microseconds." << std::endl;

    return 0;
}
#else

#include "../hdr/catch_testcases.h"

#endif
