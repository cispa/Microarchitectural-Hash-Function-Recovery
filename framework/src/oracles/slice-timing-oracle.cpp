#include <linux/perf_event.h>
#include <cassert>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdexcept>
#include <exception>
#include <plog/Log.h>
#include <sys/mman.h>
#include "../../include/oracle.hpp"
#include "../../include/utils.hpp"

#define ALLIGN_PAGE(a) (a & ~(4096-1)) + 4096


SliceTimingOracle::SliceTimingOracle(int runs, int confidence, IAddr* addr) : Oracle(runs,confidence)
{
  this->addr = addr;
  this->cores = phys_cores();
  PLOG_INFO << "Determening Treshold";
  determine_treshold();
  PLOG_INFO << "Treshold set to " << this->threshold;  
}

SliceTimingOracle::~SliceTimingOracle()
{
}

uint64_t SliceTimingOracle::oracle(pointer addr){
  uint64_t hist[this->cores] = {0};
  for (size_t i = 0; i < 5000; i++)
  {
    for (size_t c = 0; c < this->cores; c++)
    {
      pin_to_core(getpid(), c);
      size_t start = rdtsc();
      maccess((void*) addr);
      flush((void*) addr);
      size_t end = rdtsc(); 
      if(end-start < this->threshold){
        hist[c]++;
      }
    }
  }
  return std::distance(hist, std::max_element(hist,hist+this->cores));
}

void SliceTimingOracle::determine_treshold(){
  std::vector<uint64_t> measurements;
  for (size_t i = 0; i < 100000; i++)
  {
    auto rand_addr = this->addr->get_random_addr();
    auto rand_addr_mapped = (void*) this->addr->map_addr(rand_addr);
    for (size_t c = 0; c < this->cores; c++)
    {
      pin_to_core(getpid(), c);
      size_t start = rdtsc();
      maccess((void*) addr);
      flush((void*) addr);
      size_t end = rdtsc();
      measurements.push_back(end-start); 
    }
  }
  this->threshold = ostsu_treshold(measurements);
}

