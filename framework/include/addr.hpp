#ifndef _ADDR_H_
#define _ADDR_H_

#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
#include <random>
#include <cmath>
#include <map>
#include <boost/icl/interval_set.hpp>

#include "pcg_random.hpp"

typedef uint64_t pointer;

#define ADDR_RETRIES_INC 100000
#define ADDR_RETRIES_RAND_SELECT 1000

/*
 * Contexts for the mappable dram ranges, it is used to work with physical
 * addresses
 */
typedef struct dram_ctx {
  boost::icl::interval_set<uint64_t> ram_ranges;
  uint64_t ram_addresses;
  uint64_t max_address;
} dram_ctx;


/*
* Interface to the get addresses
*/
class IAddr {
 private:
  std::random_device rd; // seed the random number generator
  
 protected:
  pcg64 rnd{rd()};
  pointer bitmsask_iterator_position; // current position of the iterator
  uint64_t bitmask;
  std::vector<uint64_t> idx_vec;
  std::vector<uint64_t> idx_vec_invert;

 public:
  size_t maxbits;
  std::pair<pointer, pointer> get_flip_pair(int idx);
  void init_bitmask_iterator(std::vector<uint64_t> idx_vec);
  pointer get_alternative_addr(pointer addr);
  virtual std::pair<pointer,bool> advance_bitmask_iterator(size_t step) = 0;
  virtual bool valid_address(pointer address) = 0;
  virtual pointer get_random_addr() = 0;
  virtual pointer map_addr(pointer addr) = 0;
  virtual pointer flip_unused_bits(pointer addr) = 0;
};

/*
* Addr class for physical addresses
*/
class PhysAddr : public IAddr
{
    private:
      dram_ctx dram;
      char* map_base;
      
    public:
      virtual std::pair<pointer,bool> advance_bitmask_iterator(size_t step);
      virtual bool valid_address(pointer addr);
      virtual pointer get_random_addr();
      virtual pointer map_addr(pointer addr);
      virtual pointer flip_unused_bits(pointer addr);
      virtual void make_cachable();
      virtual void make_uncachable();
      PhysAddr();
      ~PhysAddr();
};

/*
* Addr class for virtual addresses
*/
class VirtAddr : public IAddr
{
    private:
      char* map_base;

    public:
      virtual std::pair<pointer,bool> advance_bitmask_iterator(size_t step);
      virtual bool valid_address(pointer addr);
      virtual pointer get_random_addr();
      virtual pointer map_addr(pointer addr);
      virtual pointer flip_unused_bits(pointer addr);
      VirtAddr(size_t maxbits);
      ~VirtAddr();
};

#endif

