
#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
#include <unistd.h>
#include <cassert>
#include <bitset>
#include <algorithm>
#include <sstream>
#include <string>
#include <chrono>
#include <boost/tokenizer.hpp>
#include <cstdlib>
#include <filesystem>


#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include "CLI/App.hpp"
#include "CLI/Formatter.hpp"
#include "CLI/Config.hpp"

#include "../include/addr.hpp"
#include "../include/utils.hpp"
#include "../include/framework.hpp"
#include "../include/oracle.hpp"


int main(int argc, char** argv) {
  // Parse command line arguments
  CLI::App app{"Unscatter framework."};

  int core = 0;
  app.add_option("-c,--core", core, "Core to pin framwork to");

  int set = 0;
  app.add_option("-s,--set-option", set, "Set currently implemented frameworks 0=Slice Direct, 1=Slice Indirect, 2=Utag Indirect, 3=DRAM Indirect");

  int output_classes = 0;
  app.add_option("-o,--output-classes", output_classes, "Number of output classes, (if not given then determined automatically)");

  int tresh_oracle = 9;
  app.add_option("-t,--tresh-oracle", tresh_oracle, "Treshold of oracle in percent default 90%");

  int bit_limit_hi = 64;
  app.add_option("-u,--bit-limit-upper", bit_limit_hi, "Bit limit for highest bit that gets dumped");
  
  int bit_limit_lo = 0;
  app.add_option("-l,--bit-limit-lower", bit_limit_lo, "Bit limit for lowest bit that gets dumped");

  std::string input_bits_file = "";
  app.add_option("-r,--relevant-input-bits", input_bits_file, "File containing the relevant input bits");

  bool dry_run = false;
  app.add_flag("--dry-run",dry_run,"Perform a dry run without measurements.");

  bool is_xeon = false;
  app.add_flag("--xeon",is_xeon,"Enable if tested processor is an Intel Xeon chip");

  //std::string measurement_dir = "";
  //app.add_option("-m,--measurement-result-dir", input_bits_file, "Directory for the measurement results");
  
  // Parse command line ars
  CLI11_PARSE(app,argc,argv);

  // Init logging
  static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
  plog::init(plog::debug, &consoleAppender);

  // Check if started as root
  if (geteuid()) {
    PLOG_FATAL << "Framework must be run as root";
    exit(1);
  }

  // Check if output dir exists
  if(std::filesystem::is_directory("measurements")){
    PLOG_INFO << "Delete ./measurements folde and rerun unscatter to discard previous results";
    exit(1);
  }else{
    PLOG_INFO << "Creating ./measurement folder";
    std::filesystem::create_directory("measurements");
  }
  
  // Time execution
  auto start = std::chrono::high_resolution_clock::now();


  Oracle* oracle;
  IAddr* addr;
  Framework* framework;
  switch (set)
  {
  case 0:
    PLOG_INFO << "Measuring cache slices with performance counters";
    oracle = new SliceOracle(10,tresh_oracle,is_xeon);
    addr = new PhysAddr();
    framework = new BitwiseFramework(core,addr,oracle);
    break;
  case 1: 
    PLOG_INFO << "Measuring cache slices with timing";
    addr = new PhysAddr();
    oracle = new SliceTimingOracle(10,tresh_oracle,addr);
    framework = new BitwiseFramework(core,addr,oracle);
    break;
  case 2:
    PLOG_INFO << "Measuring utag hash function";
    addr = new VirtAddr(30);
    oracle = new UtagOracle(10,tresh_oracle,addr);
    framework = new NaiveFramework(core,addr,oracle);
    output_classes = oracle->output_classes;
    break;
  case 3: 
    PLOG_INFO << "Measuring DRAM addressing function";
    addr = new PhysAddr();
    oracle = new DramaOracle(10,tresh_oracle,addr);
    framework = new NaiveFramework(core,addr,oracle);
    output_classes = oracle->output_classes;
    break;
  default:
    PLOG_ERROR << "Invalid oracle selected current choices 0-3";
    exit(1);
  }

  framework->bit_limit_hi = bit_limit_hi;
  framework->bit_limit_lo = bit_limit_lo;

  // Set output classes if given else infer
  if(output_classes == 0){
    PLOG_INFO << "Determening output classes";
    framework->determine_output_classes();
  }else{
    oracle->output_classes = output_classes;
    framework->output_classes = output_classes;
  }
  PLOG_INFO << "Output class count: " << framework->output_classes;

  if(input_bits_file == ""){
    PLOG_INFO << "Measuring relevant input bits";
    framework->get_input_space_bits();
  }else{
    PLOG_INFO << "Reading relevant input bits from " << input_bits_file;
    std::ifstream file(input_bits_file.c_str());
    typedef boost::tokenizer<boost::escaped_list_separator<char> > Tokenizer;
    std::vector<std::string> vec;
    std::string line;

    while (getline(file,line))
    {
        Tokenizer tok(line);
        vec.assign(tok.begin(),tok.end());
        std::vector<uint64_t> bits;
        for(auto elem : vec){
          bits.push_back(std::stoi(elem));
        }
        framework->input_space_bits.push_back(bits);
        framework->input_space_linear.push_back(false);
    }
  }

  PLOG_INFO << "Relevant input bits for h[x]:\n" <<framework->input_space_bits;
  PLOG_INFO << "Linear bits of h:\n" << framework->input_space_linear;
  if(dry_run){
    PLOG_INFO << "Performing dry run";
    framework->dry_run();
  }else{
    PLOG_INFO << "Dumping truth table";
    framework->dump_truth_table();
  }
  
  // Log execution time
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::seconds>(stop - start);
  PLOG_INFO << "Overall runtime " <<  duration.count() << "s";

}
