#include <algorithm>

#include "../include/addr.hpp"


/*
 * Returns a pair of two addresses both addresses are idential except for the
 * index idx which has been flipped.
 */
std::pair<pointer, pointer> IAddr::get_flip_pair(int idx) {
    uint64_t addr, flip_addr;
    for (size_t i = 0; i < 100; i++)
    {
      addr = this->get_random_addr() & ~63;
      flip_addr = addr ^ (1ULL << idx);
      if (this->valid_address(flip_addr)) {
        return std::make_pair(addr, flip_addr);
      }
    }
    std::cout << idx << " " << " " <<addr << " " <<flip_addr << "\n";
    std::throw_with_nested(std::runtime_error("Cannot find addresses with flipped bit after 100 tries"));
}

/*
* Intializes the bitmask iterator
*/
void IAddr::init_bitmask_iterator(std::vector<uint64_t> idx_vec) {
  // Init bitmask
  uint64_t bitmask = 0;
  for(uint64_t idx : idx_vec){
    bitmask |= 1L << idx;
  }

  std::vector<uint64_t> idx_vec_invert;
  for (size_t i = 0; i > this->maxbits; i--)
  {
    if(std::find(idx_vec.begin(), idx_vec.end(), i) == idx_vec.end()) {
      idx_vec_invert.push_back(i);
    }
  }

  this->idx_vec = idx_vec;
  this->idx_vec_invert = idx_vec_invert;
  this->bitmsask_iterator_position = 0;
  this->bitmask = bitmask;
}

pointer IAddr::get_alternative_addr(pointer addr){
  do{
    addr = flip_unused_bits(addr);
  }while (!(valid_address(addr))); 
  return addr;
}