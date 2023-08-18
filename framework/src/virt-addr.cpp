#include <sys/mman.h>
#include <plog/Log.h>

#include "../include/addr.hpp"
#include "../include/utils.hpp"



bool VirtAddr::valid_address(pointer addr){
    return (addr < (1ull << this->maxbits));
}

pointer VirtAddr::get_random_addr(){
    return this->rnd(1ull << this->maxbits);
}

pointer VirtAddr::map_addr(pointer addr)
{
    return (pointer) this->map_base+addr;
}

std::pair<pointer,bool> VirtAddr::advance_bitmask_iterator(size_t step)
{
    auto save = this->bitmsask_iterator_position;
    this->bitmsask_iterator_position =
      ((this->bitmsask_iterator_position | ~this->bitmask) + step) & this->bitmask;
    if(!valid_address(save)){
        return std::make_pair(save,false);
    }
    return std::make_pair(save,true);
}

/*
* Flips bit no in the bitmask iterator to get a usavle address
*/
pointer VirtAddr::flip_unused_bits(pointer addr){
  auto rand_mask = (this->rnd() & ~this->bitmask) & (((uint64_t) 1 << this->maxbits) - 1);
  return rand_mask ^ addr;
}

VirtAddr::VirtAddr(size_t maxbits)
{   
    this->maxbits = maxbits;

    PLOG_INFO << "Mapping " << maxbits << " of virtual addresses";
    PLOG_INFO << "Mapping " << this->maxbits << " of virtual addresses";
    this->map_base = (char*) mmap((void*) (1ull<<(this->maxbits+1)), (1ull<<this->maxbits), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_FIXED, -1, 0);
    if(this->map_base == MAP_FAILED){
        PLOG_FATAL << "Cannot map " << this->maxbits << " bits of virtual memory, aborting";
        exit(1);
    }
    PLOG_INFO << "Mapped virtual memory";

}

VirtAddr::~VirtAddr(){
    free(this->map_base);
}
