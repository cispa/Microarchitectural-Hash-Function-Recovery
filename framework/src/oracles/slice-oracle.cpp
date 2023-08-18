#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <cpuid.h>

#include "../../include/oracle.hpp"
#include "../../include/utils.hpp"


SliceOracle::SliceOracle(int runs, int confidence,bool is_xeon): Oracle(runs,confidence)
{
    this->is_xeon = is_xeon;
    this->cores = phys_cores();

    int name[4] = {0, 0, 0, 0};
    __cpuid(0, this->cpu_architecture, name[0], name[2], name[1]);
    if(!strcmp((char *) name, "GenuineIntel") == 0) {
        int res[4];
        __cpuid(1, res[0], res[1], res[2], res[3]);
        this->cpu_architecture = ((res[0] >> 8) & 7) + (res[0] >> 20) & 255;
    }
}

SliceOracle::~SliceOracle()
{

}

uint64_t SliceOracle::oracle(pointer addr){
    return measure_slice((void* )addr);
}

size_t SliceOracle::measure_slice(void* address) {
    if(this->is_xeon) {
        return measure_slice_xeon(address);
    } else {
        return measure_slice_core(address);
    }
}

size_t SliceOracle::measure_slice_core(void *address) {
    int msr_unc_perf_global_ctr;
    int val_enable_ctrs;
    int ctr_msr = 0x706, ctr_space = 0x10;
    int ctrl_msr = 0x700, ctrl_space = 0x10;
    int ctrl_config = 0x408f34;
    if(this->cpu_architecture >= 0x16) {
        // >= skylake   
        msr_unc_perf_global_ctr = 0xe01;
        val_enable_ctrs = 0x20000000;
        if(this->cpu_architecture >= 0x1b) {
            // >= ice lake
            ctr_msr = 0x702;
            ctr_space = 0x8;
            ctrl_space = 0x8;
            ctrl_config = 0x408834;
            if(this->cpu_architecture >= 0x20) {
                // >= alder lake
                msr_unc_perf_global_ctr = 0x2ff0;
                ctr_msr = 0x2002;
                ctr_space = 0x8;
                ctrl_space = 0x8;
                ctrl_msr = 0x2000;
            }
        }
    } else {
        msr_unc_perf_global_ctr = 0x391;
        val_enable_ctrs = 0x2000000f;
    }
    
    // Disable counters
    if(wrmsr(0, msr_unc_perf_global_ctr, 0x0)) {
        return -1ull;
    }

    // Reset counters
    for (int i = 0; i < this->cores; i++) {
        wrmsr(0, ctr_msr + i * ctr_space, 0x0);
    }

    // Select event to monitor
    for (int i = 0; i < this->cores; i++) {
        wrmsr(0, ctrl_msr + i * ctrl_space, ctrl_config);
    }

    // Enable counting
    if(wrmsr(0, msr_unc_perf_global_ctr, val_enable_ctrs)) {
        return -1ull;
    }

    // Monitor
    int access = 10000;
    while (--access) {
        flush(address);
    }

    // Read counter
    size_t cboxes[this->cores];
    for (int i = 0; i < this->cores; i++) {
        int cnt = rdmsr( 0, ctr_msr + i * ctr_space);
        if(cnt < 0) cnt = 0;
        cboxes[i] = cnt;
    }

    return find_index_of_nth_largest_size_t(cboxes, this->cores, 0);
}


size_t SliceOracle::measure_slice_xeon(void *address) {
    #define REP4(x) x x x x
    #define REP16(x) REP4(x) REP4(x) REP4(x) REP4(x) 
    #define REP256(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) REP16(x) 
    #define REP1K(x) REP256(x) REP256(x) REP256(x) REP256(x) 
    #define REP4K(x) REP1K(x) REP1K(x) REP1K(x) REP1K(x)

    char buffer[16];
    char path[1024];
    int event[512];
    memset(event, 0, sizeof(event));
    volatile int dummy = 0;
    int slices = 0;

    DIR* dir = opendir("/sys/bus/event_source/devices/");
    if(!dir) return -1;
    struct dirent* entry;
    while((entry = readdir(dir)) != NULL) {
        if(!strncmp(entry->d_name, "uncore_cha_", 11)) {
            snprintf(path, sizeof(path), "/sys/bus/event_source/devices/%s/type", entry->d_name);
            FILE* f = fopen(path, "r");
            if(!f) return -1;
            dummy += fread(buffer, 16, 1, f);
            fclose(f);
            int slice = atoi(entry->d_name + 11);
            int event_id = atoi(buffer);
            event[slice] = event_id;
            if(slice > slices) slices = slice;
        }
    }
    closedir(dir);
    
    size_t hist[slices];
    memset(hist, 0, sizeof(hist));

    int fds[slices];

    for(int i = 0; i < slices; i++) {
        fds[i] = event_open((enum perf_type_id)(event[i]), 0x1bc10000ff34, 0, 0, 0, i);
        ioctl(fds[i], PERF_EVENT_IOC_ENABLE, 0);
        ioctl(fds[i], PERF_EVENT_IOC_RESET, 0);
    }
    REP4K(asm volatile("mfence; clflush (%0); mfence; \n" : : "r" (address) : "memory"); *(volatile char*)address;)
    for(int i = 0; i < slices; i++) {
        ioctl(fds[i], PERF_EVENT_IOC_DISABLE, 0);

        long long result = 0;
        read(fds[i], &result, sizeof(result));
        hist[i] = result;
        close(fds[i]);
    }

    return find_index_of_nth_largest_size_t(hist, slices, 0);
}


