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
#include "ptedit_header.h"


#define ALLIGN_PAGE(a) (a & ~(4096-1)) + 4096
#define FIXPOINT_DRAM 100

void DramaOracle::build_oracle(){
    // Set up initial address
    std::vector<pointer> start = {ALLIGN_PAGE(this->addr->get_random_addr())};
    this->class_examples.push_back(start);

    // Iterate until fixpoint reached
    auto unchanged_iterations = 0;
    while(unchanged_iterations < FIXPOINT_DRAM){
      auto rand_addr = ALLIGN_PAGE(this->addr->get_random_addr());
      auto rand_addr_mapped = this->addr->map_addr(rand_addr);
      bool is_new_class = true;
      // Iterate over all previously seen classes
      for (size_t i = 0; i < this->class_examples.size(); i++)
      {
        auto class_mapped = this->addr->map_addr(this->class_examples[i][0]);
        if(weak_drama_oracle((void*) class_mapped, (void*) rand_addr_mapped ,this->threshold)){
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


DramaOracle::DramaOracle(int runs, int confidence, IAddr* addr) : Oracle(runs,confidence)
{
  this->addr = addr;
  PLOG_INFO << "Determening Treshold";
  determine_treshold();
  PLOG_INFO << "Treshold set to " << this->threshold;  
  build_oracle();
}

DramaOracle::~DramaOracle()
{

}

uint64_t DramaOracle::oracle(pointer addr){
  addr = ALLIGN_PAGE(addr);
  int hist[this->class_examples.size()] = {0};
  for (size_t i = 0; i < this->class_examples.size(); i++)
  {
    for(auto class_example : this->class_examples[i]){
      auto class_map = this->addr->map_addr(class_example);
      if(this->weak_drama_oracle((void*) class_map, (void*) addr,20)){
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

void DramaOracle::determine_treshold(){
  std::vector<uint64_t> measurements;
  for (size_t i = 0; i < 5000; i++)
  {
    auto rand_addr_1 = ALLIGN_PAGE(this->addr->get_random_addr());
    auto rand_addr_mapped_1 = this->addr->map_addr(rand_addr_1);
    auto rand_addr_2 = ALLIGN_PAGE(this->addr->get_random_addr());
    auto rand_addr_mapped_2 = this->addr->map_addr(rand_addr_2);
    uint64_t res = getTiming(rand_addr_mapped_1,rand_addr_mapped_2);
    measurements.push_back(res);
  }
  this->threshold = ostsu_treshold(measurements);
}

bool DramaOracle::weak_drama_oracle(void* address1, void* address2, size_t threshold)
{
  auto timing = getTiming((pointer) address1, (pointer) address2);
  return timing > threshold;
}

uint64_t DramaOracle::getTiming(pointer first, pointer second) {
    size_t min_res = (-1ull);
    for (int i = 0; i < 4; i++) {
        size_t number_of_reads = num_reads;
        volatile size_t *f = (volatile size_t *) first;
        volatile size_t *s = (volatile size_t *) second;

        for (int j = 0; j < 10; j++)
            sched_yield();
        size_t t0 = rdtsc();

        while (number_of_reads-- > 0) {
            *f;
            *(f + number_of_reads);

            *s;
            *(s + number_of_reads);

            asm volatile("clflush (%0)" : : "r" (f) : "memory");
            asm volatile("clflush (%0)" : : "r" (s) : "memory");
        }

        uint64_t res = (rdtsc2() - t0) / (num_reads);
        for (int j = 0; j < 10; j++)
            sched_yield();
        if (res < min_res)
            min_res = res;
    }
    return min_res;
}

