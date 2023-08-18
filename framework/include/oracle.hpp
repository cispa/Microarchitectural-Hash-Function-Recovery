#ifndef _ORACLE_H_
#define _ORACLE_H_

#include <cstdint>
#include <vector>

#include "addr.hpp"

#define MAX_OUTPUT_CLASS_ASSUMPTION 100

typedef uint64_t pointer;

/*
* Generic oracle class that can be used to model arbitrary oracles
*/

class Oracle
{
    private: 
    public:
        int runs;
        int confidence;
        int precision; // precision parameter that can be used to tweak oracle
        uint64_t oracle_robust(pointer addr, pointer paddr, int retries);
        virtual uint64_t oracle(pointer addr) = 0;
        uint64_t output_classes;
        Oracle(int runs,int confidence);
};

class SliceOracle : public Oracle
{
private:
    bool is_xeon;
    int cores;
    int cpu_architecture;
    size_t measure_slice(void* address);
    size_t measure_slice_core(void* address);
    size_t measure_slice_xeon(void* address);

public:
    SliceOracle(int runs,int confidence,bool is_xeon);
    ~SliceOracle();
    uint64_t oracle(pointer addr);
};

class UtagOracle : public Oracle
{
private:
    std::vector<std::vector<pointer>> class_examples;
    int pc_l1d_read_miss;
    IAddr* addr;
    bool weak_utag_oracle(void* address1, void* address2, size_t threshold);
    void build_oracle();

public:
    UtagOracle(int runs,int confidence,IAddr* addr);
    ~UtagOracle();
    uint64_t oracle(pointer addr);
};

class DramaOracle : public Oracle
{
private:
    std::vector<std::vector<pointer>> class_examples;
    IAddr* addr;
    size_t threshold;
    size_t num_reads = 5000;
    bool weak_drama_oracle(void* address1, void* address2, size_t threshold);
    void build_oracle();
    void determine_treshold();
    uint64_t getTiming(pointer first, pointer second);

public:
    DramaOracle(int runs,int confidence,IAddr* addr);
    ~DramaOracle();
    uint64_t oracle(pointer addr);
};

class SliceTimingOracle : public Oracle
{
private:
    IAddr* addr;
    size_t threshold;
    int cores;
    void determine_treshold();
    
public:
    SliceTimingOracle(int runs,int confidence,IAddr* addr);
    ~SliceTimingOracle();
    uint64_t oracle(pointer addr);
};


#endif
