#include <algorithm>
#include <iostream>
#include <plog/Log.h>


#include "../../include/oracle.hpp"

Oracle::Oracle(int runs, int confidence){
  this->runs = runs;
  this->confidence = confidence;
}

uint64_t Oracle::oracle_robust(pointer addr, pointer paddr, int retries){
    if(this->output_classes == 0){
        std::throw_with_nested(std::runtime_error("Set output classes before starting robust measurement, output classes can also be set highter than required\n"));
    }
  #ifdef DEBUG
    std::cout << "\r" << std::hex << addr << " [0x" << paddr << "] " << std::dec;
  #endif
  for (size_t t = 0; t < retries; t++)
  {
    std::vector<uint64_t> hist(std::max(this->output_classes,(uint64_t)MAX_OUTPUT_CLASS_ASSUMPTION),0);
    for (size_t i = 0; i < this->runs; i++)
    {
      hist[this->oracle(addr)]++;
    }
    auto it = std::max_element(hist.begin(), hist.end());
    if(*it > this->confidence){
      return std::distance(hist.begin(), it);
    }
    
    PLOG_DEBUG << "Remeasure triggered for 0x" << std::hex << addr << " [0x" << paddr << "] " << std::dec << " max was " << *it << "/" << runs;
  }
  std::throw_with_nested(std::runtime_error("Maximum retries for robust oracle exceeded.\n"));
  return 0; // Unreachable
}
