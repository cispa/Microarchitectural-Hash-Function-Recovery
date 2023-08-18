
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
#include <boost/tokenizer.hpp>

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

#define THRESHOLD_FIXPOINT_ITERATION 500
#define ITERATIONS_INPUT_SPACE_MEASURE 10
#define WEAK_ORACLE_FIXPOINT_ITERATON 100
#define MAX_RETRIES_ORACLE 10000
#define MULTIMEASURE

/*
 * Peroforms fixpoint iteration with threshold to find the number of output
 * classes.
 * If concrete output classes are known beforehand this part can be skipped.
 * To skip this step just overwrite the output classes of the framework direcly.
 */
void NaiveFramework::determine_output_classes() {
  std::set<uint64_t> oracle_outputs;
  uint64_t unchanged_iterations = 0;
  while (unchanged_iterations < THRESHOLD_FIXPOINT_ITERATION) {
    auto size_before = oracle_outputs.size();
    auto random_addr = this->addr->get_random_addr();
    auto mapped_addr = this->addr->map_addr(random_addr);
    oracle_outputs.insert(this->oracle->oracle(mapped_addr));
    auto size_after = oracle_outputs.size();
    if (size_before == size_after) {
      unchanged_iterations++;
    } else {
      unchanged_iterations = 0;
    }
  }
  this->output_classes = oracle_outputs.size();
  this->oracle->output_classes = this->output_classes;
}

/*
 * Returns the bit indices relevant for the output.
 * The precision of this measurement can be increased by increasing the
 * threshold parameter
 */
void NaiveFramework::get_input_space_bits() {
    std::vector<uint64_t> out_class_bits;
    PLOG_INFO << "Computing relevant input space";
    PLOG_DEBUG << this->addr->maxbits;
    // Try each address bit that can be mapped
    bool linear = true;
    for (size_t addr_bit= 0; addr_bit < this->addr->maxbits; addr_bit++) {
      // True and false counts to determine linearity
      int true_cnt = 0;
      int false_cnt = 0;
      bool addr_bit_relevant = false;
      // Perform iterations to determine likelyhood that the input bit influences the output result
      for (size_t iteration = 0; iteration < ITERATIONS_INPUT_SPACE_MEASURE;
           iteration++) {
        // Get a pair of addresses where the bit index addr_bit is flipped between the two addresses
        auto pair = this->addr->get_flip_pair(addr_bit);
        auto addr_1 = pair.first;
        auto addr_2 = pair.second;
        auto addr_1_mapped = this->addr->map_addr(addr_1);
        auto addr_2_mapped = this->addr->map_addr(addr_2);

        auto first_measure = this->oracle->oracle_robust(addr_1_mapped, addr_1, MAX_RETRIES_ORACLE);
        auto second_measure = this->oracle->oracle_robust(addr_2_mapped, addr_2, MAX_RETRIES_ORACLE);

        if(first_measure != second_measure){
          addr_bit_relevant = true;
          true_cnt++;
        }else{
          false_cnt++;
        }        
      }
      PLOG_DEBUG <<"Bit:" << addr_bit << " rel/irel:" << true_cnt << "/" << false_cnt;
      if (addr_bit_relevant) {
        out_class_bits.push_back(addr_bit);
        linear &= (true_cnt == ITERATIONS_INPUT_SPACE_MEASURE);
      }
    }
    // If there are relevant bits record them
    if(out_class_bits.size() > 0){
      this->input_space_bits.push_back(out_class_bits);
      this->input_space_linear.push_back(linear);
    }

    // Dump relevant input space bits
    std::ofstream bitfile;
    bitfile.open("measurements/bits.csv");

    for(auto input_space : this->input_space_bits){
      if(input_space_bits.size() != 0){
        for (size_t i = 0; i < input_space.size(); i++)
        {
          if(input_space[i] >= this->bit_limit_hi || input_space[i] <= this->bit_limit_lo){continue;}
          if(i != input_space.size()-1){
            bitfile << input_space[i] << ", "; 
          }else{
            bitfile << input_space[i];
          }
        }
        bitfile << "\n";
      }
    }
}

/*
* Dumps truth tables for all relevant bits
*/
void NaiveFramework::dump_truth_table(){

  // Open dumpfile
  std::ofstream dumpfile;
  dumpfile.open("measurements/allbits.csv");
  dumpfile << "addr,class\n";

  // Init bitmask iterator
  this->addr->init_bitmask_iterator(this->input_space_bits[0]);

  // Reduce by applying lower and upper bit bounds
  auto reduce = 0;
  std::vector<uint64_t> bit_idx_reduce;
  std::vector<uint64_t> bit_idx_expand;
  for(auto b: this->input_space_bits[0]){
    if(b >= this->bit_limit_hi || b <= this->bit_limit_lo){
      reduce++;
      bit_idx_expand.push_back(b);
    }else{
      bit_idx_reduce.push_back(b);
    }
  }
  size_t iterations = 1L << (this->input_space_bits[0].size()-reduce);

  
  PLOG_INFO << "Dumping h naively, iteration count " << iterations;

  // Iterate over addresses dumping truth table
  int perc = 0, last_perc = -1;
  for (size_t i = 0; i < iterations ; i++)
  {
    // Try to determine output class of an address
    auto addr_tuple = this->addr->advance_bitmask_iterator(1ULL << (bit_idx_reduce[0]));
    pointer select_addr = addr_tuple.first;
    bool can_map = addr_tuple.second;
    if(!can_map){
      dumpfile << select_addr << ", "<< "-" << "\n";
    }else{
      int count = 0;
      // Try to map an address
      auto mapped_addr = this->addr->map_addr(select_addr);
      // Try to measure until succesfull
      bool oracle_success = false;
      uint64_t oracle_out = 0;
      while (!oracle_success)
      {
        try{
        // This can error if the maximum retry  is exceeded
        oracle_out = this->oracle->oracle_robust(mapped_addr,select_addr,10); 
        //std::cout << ((oracle_out>>bit_idx) & 0x1) << "\n"; 
        dumpfile << select_addr << ", "<< oracle_out << "\n";
        oracle_success = true;
        }catch (...){
          PLOG_DEBUG << "REMEASURE triggerd while dumping";
        }
      }
    }
      
    perc = (int)(i * 100.0 / iterations);
    if(perc != last_perc) {
      PLOG_DEBUG << perc <<"% " << i << "/" << iterations;
      last_perc = perc;
    }
  }
}

/*
 * Performs a dry run without measurements, good for debugging
 */
void NaiveFramework::dry_run(){
  uint64_t unmappable = 0;
  // Init bitmask iterator
  this->addr->init_bitmask_iterator(this->input_space_bits[0]);

  // Reduce by applying lower and upper bit bounds
  auto reduce = 0;
  std::vector<uint64_t> bit_idx_reduce;
  std::vector<uint64_t> bit_idx_expand;
  for(auto b: this->input_space_bits[0]){
    if(b >= this->bit_limit_hi || b <= this->bit_limit_lo){
      reduce++;
      bit_idx_expand.push_back(b);
    }else{
      bit_idx_reduce.push_back(b);
    }
  }
  size_t iterations = 1L << (this->input_space_bits[0].size()-reduce);

  PLOG_INFO << "Dry run on h naive iteration count " << iterations;
  // Iterate over addresses dumping truth table
  int perc = 0, last_perc = -1;
  for (size_t i = 0; i < iterations ; i++)
  {
    // Try to determine output class of an address
    auto addr_tuple = this->addr->advance_bitmask_iterator(1ULL << (bit_idx_reduce[0]));
    pointer select_addr = addr_tuple.first;
    bool can_map = addr_tuple.second;
    if(!can_map){
      unmappable++;
    }
      
    perc = (int)(i * 100.0 / iterations);
    if(perc != last_perc) {
      PLOG_DEBUG << perc <<"% " << i << "/" << iterations;
      last_perc = perc;
    }
  }
  PLOG_INFO << unmappable << " Addresses not mappable when dumping all function bits";
}

/*
 * Initializer for the framework that allows to pin the code to a core.
 */
NaiveFramework::NaiveFramework(int core, IAddr* addr, Oracle* oracle) {
  // Set selected oracle and address type
  this->addr = addr;
  this->oracle = oracle;
  // Pin to selected core
  cpu_set_t mask;
  mask.__bits[0] = 1 << core;
  sched_setaffinity(getpid(), sizeof(cpu_set_t), &mask);
}

NaiveFramework::~NaiveFramework() {
  delete this->addr;
  delete this->oracle;
}

