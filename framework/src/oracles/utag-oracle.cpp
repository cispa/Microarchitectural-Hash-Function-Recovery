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
#define FIXPOINT_UTAG 1000
#define UTAG_REDUCE_MAX 20

void UtagOracle::build_oracle(){
    // Set up initial address
    std::vector<pointer> start = {ALLIGN_PAGE(this->addr->get_random_addr())};
    this->class_examples.push_back(start);

    // Iterate until fixpoint reached
    auto unchanged_iterations = 0;
    while(unchanged_iterations < FIXPOINT_UTAG){
      auto rand_addr = ALLIGN_PAGE(this->addr->get_random_addr());
      auto rand_addr_mapped = this->addr->map_addr(rand_addr);
      bool is_new_class = true;
      // Iterate over all previously seen classes
      for (size_t i = 0; i < this->class_examples.size(); i++)
      {
        auto class_mapped = this->addr->map_addr(this->class_examples[i][0]);
        if(weak_utag_oracle((void*) class_mapped, (void*) rand_addr_mapped ,20)){
          // If example is already in classes stop
          is_new_class = false;
          this->class_examples[i].push_back(rand_addr);
          break;
        }
      }

      if(is_new_class){
        std::vector<pointer> new_class = {rand_addr};
        this->class_examples.push_back(new_class);
        PLOG_INFO << "Class count:" << this->class_examples.size(); 
        unchanged_iterations = 0;
      }
      unchanged_iterations++;
    }

    // Filter classes only containing one element
    this->class_examples.erase(std::remove_if(this->class_examples.begin(), this->class_examples.end(), 
      [](std::vector<pointer> v) { return v.size() <= 1; }), this->class_examples.end());
    PLOG_INFO << "Class count filtered:" << this->class_examples.size(); 

    // Print solution 
    // for (std::vector<pointer> p : this->class_examples)
    // {
    //   std::cout << p.size() << " ";
    // }
    // std::cout << "\n";
    this->output_classes = class_examples.size();
}


UtagOracle::UtagOracle(int runs, int confidence, IAddr* addr) : Oracle(runs,confidence)
{
  this->addr = addr;
  this->pc_l1d_read_miss = -1;
  build_oracle();
}

UtagOracle::~UtagOracle()
{
    
}

uint64_t UtagOracle::oracle(pointer addr){
  addr = ALLIGN_PAGE(addr);
  int hist[this->class_examples.size()] = {0};
  for (size_t i = 0; i < this->class_examples.size(); i++)
  {
    for(auto class_example : this->class_examples[i]){
      auto class_map = this->addr->map_addr(class_example);
      if(this->weak_utag_oracle((void*) class_map, (void*) addr,20)){
        hist[i]++;
      }
    }
  }
  // Normalize 
  for (size_t i = 0; i < this->class_examples.size(); i++)
  {
    hist[i] /= this->class_examples[i].size();
  }
  return std::distance(hist, std::max_element(hist,hist+this->class_examples.size()));
}

bool UtagOracle::weak_utag_oracle(void* address1, void* address2, size_t threshold)
{
  /* Open performance counter */
  if (pc_l1d_read_miss == -1) {
    size_t type = PERF_CACHE_TYPE(PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS);
    pc_l1d_read_miss = performance_counter_open(getpid(), PERF_TYPE_HW_CACHE, type);
  }

  /* Try to read */
#define TRIES (3)
#define HAMMER_TIMES (20)

  std::vector<size_t> measurements;

  performance_counter_disable(pc_l1d_read_miss);
  performance_counter_reset(pc_l1d_read_miss);
  performance_counter_enable(pc_l1d_read_miss);

  for (size_t t = 0; t < TRIES; t++) {
    size_t begin = performance_counter_read(pc_l1d_read_miss);
    mfence();

    for (size_t i = 0; i < HAMMER_TIMES; i++) {
        maccess(address1);
        mfence();
        maccess(address2);
        mfence();
    }

    nospec();

    size_t end = performance_counter_read(pc_l1d_read_miss);
    measurements.push_back(end - begin);
  }

  /* Return median */
  std::nth_element(measurements.begin(), measurements.begin() + measurements.size()/2, measurements.end());
  //if(measurements[measurements.size()/2] > threshold){
  //  PLOG_INFO << measurements << " " << measurements[measurements.size()/2];
  //}
  return measurements[measurements.size()/2] > threshold;
}