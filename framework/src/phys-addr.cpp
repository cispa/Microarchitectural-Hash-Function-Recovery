#include <iterator>
#include <bitset>
#include <algorithm>
#include <plog/Log.h>

#include "ptedit_header.h"

#include "../include/addr.hpp"
#include "../include/utils.hpp"

#define FIRST_LEVEL_ENTRIES 256 // only 256, because upper half is kernel

/*
* True if present bit is set
*/
int is_present(size_t entry) {
  return entry & (1ull << PTEDIT_PAGE_BIT_PRESENT);
}

/*
* True if page has 4K size
*/
int is_normal_page(size_t entry) {
  return !(entry & (1ull << PTEDIT_PAGE_BIT_PSE));
}

/*
* Filters page table entries to exclude the ones that are not mapped as writeback
*/
boost::icl::interval_set<pointer> get_page_blacklist(){
  boost::icl::interval_set<pointer> blacklist;
  int uc_mt = ptedit_find_first_mt(PTEDIT_MT_WB);

  size_t root = ptedit_get_paging_root(0);
  size_t pagesize = ptedit_get_pagesize();
  size_t pml4[pagesize / sizeof(size_t)], pdpt[pagesize / sizeof(size_t)],
      pd[pagesize / sizeof(size_t)], pt[pagesize / sizeof(size_t)];

  ptedit_read_physical_page(root / pagesize, (char *)pml4);

  int pml4i, pdpti, pdi, pti;

  /* Iterate through PML4 entries */
  for (pml4i = FIRST_LEVEL_ENTRIES; pml4i < 512; pml4i++) {
    size_t pml4_entry = pml4[pml4i];
    if (!is_present(pml4_entry))
      continue;

    /* Iterate through PDPT entries */
    ptedit_read_physical_page(ptedit_get_pfn(pml4_entry), (char *)pdpt);
    for (pdpti = 0; pdpti < 512; pdpti++) {
      size_t pdpt_entry = pdpt[pdpti];
      if (!is_present(pdpt_entry))
        continue;

      /* Iterate through PD entries */
      ptedit_read_physical_page(ptedit_get_pfn(pdpt_entry), (char *)pd);
      for (pdi = 0; pdi < 512; pdi++) {
        size_t pd_entry = pd[pdi];
        if (!is_present(pd_entry))
          continue;

        /* Normal 4kb page */
        if (is_normal_page(pd_entry)) {
          /* Iterate through PT entries */
          ptedit_read_physical_page(ptedit_get_pfn(pd_entry), (char *)pt);
          for (pti = 0; pti < 512; pti++) {
            size_t pt_entry = pt[pti];
            if (!is_present(pt_entry))
              continue;
            // Remove uncachable pages
            if(ptedit_extract_mt(pt_entry) != uc_mt){
              size_t uc_addr = ptedit_get_pfn(pt_entry)*4096;
              blacklist += boost::icl::discrete_interval<pointer>::closed(uc_addr, uc_addr+4096);
            }
          }
        }
      }
    }
  }
  return blacklist;
}

/*
 * Parses dram regions on the system and returns a dram context than can be used
 * to map physical adresses.
 */
dram_ctx get_dram_ctx() {
  std::ifstream indata;
  indata.open("/proc/iomem");
  if (!indata) {
    std::throw_with_nested(
        std::runtime_error("/proc/iomem could not be opened"));
  }

  boost::icl::interval_set<pointer> ram_ranges;
  uint64_t ram_addresses = 0;

  std::string line = "";
  std::string start = "";
  const char* cStart;
  std::string end = "";
  const char* cEnd;
  std::string key = "";
  char* endPointer;

  while (getline(indata, line)) {
    if (line.find("System RAM") == 0) {
      continue;
    }
    if (line.find("\n") != std::string::npos) {
      line = line.substr(0, line.find("\n"));
    }

    // parse range
    start = line.substr(0, line.find("-"));
    cStart = start.c_str();
    line = line.substr(line.find("-") + 1);
    end = line.substr(0, line.find(" : "));
    cEnd = end.c_str();
    key = line.substr(line.find(" : ") + 3);

    if (key == "System RAM") {
      ram_ranges += boost::icl::discrete_interval<pointer>::closed(strtoull(cStart, &endPointer, 16), strtoull(cEnd, &endPointer, 16));
    }
  }

  // Generate blacklist of uncachable addresses and remove them
  auto blacklist = get_page_blacklist();
  ram_ranges = ram_ranges - blacklist;

  for (auto pair : ram_ranges) {
    ram_addresses += pair.upper() - pair.lower();
  }

  uint64_t max_addr = 0;
  for(auto range: ram_ranges){
    max_addr = std::max(max_addr,range.upper());
  }

  PLOG_DEBUG << "Unmappable DRAM addresses: " <<max_addr-ram_addresses << "/" << (((float)(max_addr-ram_addresses)/(float)max_addr)*100) << "%";
  return dram_ctx{ram_ranges, ram_addresses, max_addr};
}


/*
 * Init structures needed to map physical addresses
 * Addr set size defines the number of addresses that can be mapped in parralel
 */
PhysAddr::PhysAddr() {
  // Init pteditor
  ptedit_init();
  // Get fitlered dram addresses
  this->dram = get_dram_ctx();

  // Print accesible memory ranges
  PLOG_DEBUG << std::hex << this->dram.ram_ranges << std::dec;
  for(auto range : this->dram.ram_ranges){
    PLOG_DEBUG<< std::hex << range << std::dec <<": " <<(range.upper() - range.lower()) << "b";
  }
  
  this->maxbits = ceil(log2(this->dram.ram_addresses));
  PLOG_INFO <<  this->maxbits << " bits of physical memory can be mapped ";
  
  // Map all availiable physical memory
  int fd = open("/dev/mem", O_RDONLY);
  if(fd < 3) {
      PLOG_FATAL << "Could not open /dev/mem! Did you load the kernel module and start as root?";
      exit(1);
  }

  this->map_base = (char*) mmap(0, this->dram.ram_addresses*2, PROT_READ, MAP_SHARED, fd, 0);
  assert(this->map_base != MAP_FAILED);
  if(this->map_base == (char*)-1 || this->map_base == NULL) {
      PLOG_FATAL << "Could not map physical memory!";
      exit(1);
  }
  PLOG_INFO << "Physical memory mapped at base address: 0x"<< std::hex << (uint64_t) this->map_base << std::dec;
}

/*
 * Remove ptedit
 */
PhysAddr::~PhysAddr() {
  munmap(this->map_base, this->dram.ram_addresses*2);
  ptedit_cleanup();
}

/*
 * Checks if the address maps to a valid dram range
 */
bool PhysAddr::valid_address(pointer addr){
    return this->dram.ram_ranges.find(addr) != this->dram.ram_ranges.end();
}


/*
* Maps an address and returns a pointer to the mapping
*/
pointer PhysAddr::map_addr(pointer addr){
  return (pointer)(this->map_base + addr);
}

void PhysAddr::make_uncachable(){
  PLOG_INFO << "Making memory uncachable";
  int uc_mt = ptedit_find_first_mt(PTEDIT_MT_UC);
  for(auto range: dram.ram_ranges){
    for(size_t offset = range.lower(); offset < range.upper(); offset+=4096){
        pointer vaddr = map_addr(offset);
        ptedit_entry_t entry = ptedit_resolve((void*)vaddr, 0);
        entry.pte = ptedit_apply_mt(entry.pte, uc_mt);
        entry.valid = PTEDIT_VALID_MASK_PTE;
        ptedit_update((void*)vaddr, 0, &entry);
    }
  }
}

void PhysAddr::make_cachable(){
  PLOG_INFO << "Making memory cachable";
  int wb_mt = ptedit_find_first_mt(PTEDIT_MT_WB); 
  for(auto range: dram.ram_ranges){
    for(size_t offset = range.lower(); offset < range.upper(); offset+=4096){
        pointer vaddr = map_addr(offset);
        ptedit_entry_t entry = ptedit_resolve((void*)vaddr, 0);
        entry.pte = ptedit_apply_mt(entry.pte, wb_mt);
        entry.valid = PTEDIT_VALID_MASK_PTE;
        ptedit_update((void*)vaddr, 0, &entry);
    }
  }
}

/*
 * Returns a random physical address
 * This can potentially be optimized by directky returning a valid address from one of the intervals
 */
pointer PhysAddr::get_random_addr(){
    // Select address smaller than max dram range
    int range_select = this->rnd(this->dram.ram_ranges.iterative_size());
    auto range_it = *std::next(this->dram.ram_ranges.begin(), range_select);
    auto range_delta = range_it.upper()-range_it.lower();
    int low = contains(range_it, lower(range_it)) ? 0 : 1;
    int up = contains(range_it, upper(range_it)) ? 0 : 1;
    pointer addr = (range_it.lower() + low) + (this->rnd(range_delta-up));
   
    if(!valid_address(addr)){
        PLOG_FATAL << "Invalid physical address selected: " << range_select << " " << range_it << " " << addr << " " << (addr&0x11111) << " " << range_delta << "\n";
        exit(1);
    }
    return addr;
}


/*
* Advances the bitmask iterator returning an address
*/
std::pair<pointer,bool> PhysAddr::advance_bitmask_iterator(size_t step) {

  auto addr=this->bitmsask_iterator_position;

  auto addr_max_bm = this->bitmsask_iterator_position;
  for(auto idx : this->idx_vec_invert){
      addr_max_bm |= (1L << idx);
  }
  auto bitmask_range = boost::icl::discrete_interval<pointer>::closed(addr, addr_max_bm);
  auto intersect = bitmask_range & this->dram.ram_ranges;
  if(intersect.empty()){
    this->bitmsask_iterator_position = ((this->bitmsask_iterator_position | ~this->bitmask) + step) & this->bitmask;
    //PLOG_ERROR << "Address [" << std::hex << this->bitmsask_iterator_position << std::dec <<"] cannot be mapped";
    return std::make_pair(addr,false);
  }

  auto addr_new = addr;
  while(!(valid_address(addr_new))){
    addr_new = flip_unused_bits(addr);
    PLOG_VERBOSE << "Flipping physical address bits from " << std::hex << addr << std::dec << " to " << std::hex << addr_new << std::dec;
  }
  addr=addr_new;

  // increment bitmask
  this->bitmsask_iterator_position = ((this->bitmsask_iterator_position | ~this->bitmask) + step) & this->bitmask;
  // Return found address
  return std::make_pair(addr,true);
}

/*
* Flips bit no in the bitmask iterator to get a usable address
*/
pointer PhysAddr::flip_unused_bits(pointer addr){
  auto rand_mask = (this->rnd() & ~this->bitmask) & (((uint64_t) 1 << this->maxbits) - 1);
  return rand_mask ^ addr;
}
