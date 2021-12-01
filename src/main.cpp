#include "../hdr/histogram.h"
#include "../hdr/config.h"

int main(int argc, char *argv[])
{
    std::unique_ptr<RkConfig> config = std::make_unique<RkConfig>([](config_data &d, boost::program_options::options_description &desc){
        desc.add_options()
                ("-b", boost::program_options::value<std::uint16_t>(&d.bins)->default_value(256), "# of bins in histogram (int)")
                ("-min", boost::program_options::value<double>(&d.min)->default_value(0.0), "Value at low end of histogram. Defaults to lowest value found in input nrrd. (double)")
                ("-max", boost::program_options::value<double>(&d.max)->default_value(255.0), "Value at high end of histogram. Defaults to highest value found in input nrrd. (double)")
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

    std::unique_ptr<Task> task = std::make_unique<ComputeHistogram>(config);
    task->Solve();

    return 0;
}
