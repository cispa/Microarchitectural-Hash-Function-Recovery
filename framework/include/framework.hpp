#ifndef _FRAMEWORK_H_
#define _FRAMEWORK_H_

#include "../include/addr.hpp"
#include "../include/oracle.hpp"

typedef uint64_t pointer;

/*
 * Framwork class that can be generalized to infer arbitrary microarchitectual
 * hash functions
 */
class Framework {
 protected:
  Oracle* oracle; // Oracle used for measuremnts
  IAddr* addr; // Address pool to measure on 

 public:
  uint64_t bit_limit_hi; // lowest considered bit
  uint64_t bit_limit_lo; // highest considered bit
  uint64_t output_classes; // number of output classes
  virtual void determine_output_classes() = 0;
  virtual void get_input_space_bits() = 0;
  virtual void dump_truth_table() = 0;
  virtual void dry_run() = 0;

  std::vector<std::vector<uint64_t>> input_space_bits;
  std::vector<bool> input_space_linear;

};

/*
* Decompose the function into bitwise parts, saving on the ammount of measuremnts that neeed to be performend
* Requires that the used oracle is a direct one
*/
class BitwiseFramework : public Framework{
  public:
   void determine_output_classes();
   void get_input_space_bits();
   void dump_truth_table();
   void dry_run();
   BitwiseFramework(int core, IAddr* addr, Oracle* oracle);
   ~BitwiseFramework();
};

/*
* Naive implementation that measures all bits at once
*/
class NaiveFramework : public Framework{
  public:
   void determine_output_classes();
   void get_input_space_bits();
   void dump_truth_table();
   void dry_run();
   NaiveFramework(int core, IAddr* addr, Oracle* oracle);
   ~NaiveFramework();
};

#endif