#ifndef _UTILS_H_
#define _UTILS_H_

#include <vector>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <sys/ioctl.h>
#include <stdexcept>
#include <exception>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <linux/hw_breakpoint.h> /* Definition of HW_* constants */
#include <sys/syscall.h>         /* Definition of SYS_* constants */
#include <unistd.h>

/*
 * Function that prints an array
 */
template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& v) {
  out << "{";
  size_t last = v.size() - 1;
  for (size_t i = 0; i < v.size(); ++i) {
    out << v[i];
    if (i != last) out << ", ";
  }
  out << "}";
  return out;
}

/*
 * Function that prints a nested array
 */
template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<std::vector<T>>& v) {
  out << "{";
  size_t last = v.size() - 1;
  for (size_t i = 0; i < v.size(); ++i) {
    out << v[i];
    if (i != last) out << "\n";
  }
  out << "}";
  return out;
}

/*
 * Function that prints a pair
 */
template <typename T, typename U>
std::ostream& operator<<(std::ostream& out, const std::pair<T,U>& p) {
  out << "(" << p.first << "," << p.second << ")";
  return out;
}

/*
* Gets the index in the total mesurement series based on the bitmask vector and address
*/
static uint64_t idx_from_idx_vec_and_addr(std::vector<uint64_t> idx_vec, pointer addr){
  uint64_t idx = 0;
  for (size_t i = 0; i < idx_vec.size(); i++)
  {
    idx |= (((addr >> idx_vec[i]) & 0x1) << i);
  }
  return idx;
}

/*
* Function that performs a memory access on the specified virtual address
*/
static void maccess(void *p) { asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax"); }

// ----------------------------------------------
static void nospec() { asm volatile("lfence"); }

// ----------------------------------------------
static void mfence() { asm volatile("mfence"); }

// ----------------------------------------------
static void flush(void* p) {
    asm volatile ("clflush 0(%0)\n"
      :
      : "c" (p)
      : "rax");
}

// ----------------------------------------------
static uint64_t rdtsc() {
    uint64_t a, d;
    asm volatile ("xor %%rax, %%rax\n" "cpuid"::: "rax", "rbx", "rcx", "rdx");
    asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "rcx");
    a = (d << 32) | a;
    return a;
}

// ----------------------------------------------
static uint64_t rdtsc2() {
    uint64_t a, d;
    asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "rcx");
    asm volatile ("cpuid"::: "rax", "rbx", "rcx", "rdx");
    a = (d << 32) | a;
    return a;
}

// ----------------------------------------------
static void pin_to_core(pid_t pid, int core) {
    cpu_set_t mask;
    mask.__bits[0] = 1 << core;
    sched_setaffinity(pid, sizeof(cpu_set_t), &mask);
}

// ----------------------------------------------
static int phys_cores() {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        return 1;
    }
    char *line = NULL;

    int cores[256] = {0};
    size_t len = 0;
    while (getline(&line, &len, f) != -1) {
        if(strncmp(line, "core id", 7) == 0) {
            int id = 0;
            sscanf(strrchr(line, ':') + 1, "%d", &id);
            if(id >= 0 && id < 256) {
                cores[id]++;
            }
        }
    }
    free(line);
    fclose(f);

    int phys_cores = 0;
    for(int i = 0; i < 256; i++) {
        if(cores[i]) {
            phys_cores++;
        }
    }
  return phys_cores;
}

// ----------------------------------------------
static uint64_t findMedianSorted(std::vector<uint64_t>& v,size_t offset) {
    if ((v.size()-offset) % 2 == 0) {
        return (v[(v.size()+offset) / 2 - 1] + v[(v.size()+offset) / 2]) / 2;
    } else {
        return v[(v.size()+offset) / 2];
    }
}

// ----------------------------------------------
static uint64_t ostsu_treshold(std::vector<uint64_t> measures){
  
  // Prefilter to remove outlier
  std::sort(measures.begin(), measures.end());
  auto Q1 = findMedianSorted(measures,0);
  auto Q3 = findMedianSorted(measures,measures.size()/2);
  auto IQR = Q3 - Q1;

  std::vector<uint64_t>::iterator it = measures.begin();
  while (it != measures.end()) {
      if (*it < Q1 - 1.5 * IQR || *it > Q3 + 1.5 * IQR) {
          it = measures.erase(it);
      } else {
          ++it;
      }
  }

  // Normalize remaining vector entries
  auto min = *measures.begin();
  auto max = *measures.end();
  for (size_t i = 0; i < measures.size(); i++)
  {
    measures[i] = ((measures[i]-min)*255)/((float)(max-min));
  }

  // Build a histogram for the vector
  std::vector<uint64_t> hist(255,0);
  for (size_t i = 0; i < measures.size(); i++)
  {
    hist[measures[i]]++;  
  }

  // Compute threshold
  // Init variables
  float sum = 0;
  float sumB = 0;
  int q1 = 0;
  int q2 = 0;
  float varMax = 0;
  int threshold = 0;

  // Auxiliary value for computing m2
  for (int i = 0; i <= 255; i++){
    sum += i * ((int)hist[i]);
  }

  for (int i = 0 ; i <= 255 ; i++) {
    // Update q1
    q1 += hist[i];
    if (q1 == 0) continue;

    // Update q2
    q2 = measures.size() - q1;
    if (q2 == 0) break;

    // Update m1 and m2
    sumB += (float) (i * ((int)hist[i]));
    float m1 = sumB / q1;
    float m2 = (sum - sumB) / q2;

    // Update the between class variance
    float varBetween = (float) q1 * (float) q2 * (m1 - m2) * (m1 - m2);

    // Update the threshold if necessary
    if (varBetween > varMax) {
      varMax = varBetween;
      threshold = i;
    }
  }
  
  // Denormalize treshold to get usable value
  uint64_t denormalized_threshold = ((threshold*(max-min))/255)+min;

  return denormalized_threshold;
  
}

static size_t rdmsr(int cpu, uint32_t reg) {
  char msr_file_name[64];
  sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
  
  int fd = open(msr_file_name, O_RDONLY);
  if(fd < 0) {
    return -1;
  }
  
  size_t data = 0;
  if(pread(fd, &data, sizeof(data), reg) != sizeof(data)) {
      close(fd);
      return -1;
  }

  close(fd);

  return data;
}

static int wrmsr(int cpu, uint32_t reg, uint64_t val) {
  char msr_file_name[64];
  sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
  
  int fd = open(msr_file_name, O_WRONLY);
  if(fd < 0) {
    return 1;
  }
  
  if(pwrite(fd, &val, sizeof(val), reg) != sizeof(val)) {
      close(fd);
      return 1;
  }

  close(fd);

  return 0;
}

static int find_index_of_nth_largest_size_t(size_t* list, size_t nmemb, size_t skip) {
    size_t sorted[nmemb];
    size_t idx[nmemb];
    size_t i, j;
    size_t tmp;
    memset(sorted, 0, sizeof(sorted));
    for(i = 0; i < nmemb; i++) {
        sorted[i] = list[i];
        idx[i] = i;
    }
    for(i = 0; i < nmemb; i++) {
        int swaps = 0;
        for(j = 0; j < nmemb - 1; j++) {
            if(sorted[j] < sorted[j + 1]) {
                tmp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = tmp;
                tmp = idx[j];
                idx[j] = idx[j + 1];
                idx[j + 1] = tmp;
                swaps++;
            }
        }
        if(!swaps) break;
    }

    return idx[skip];
}



/*
* Performance countner stuff
*/
#define PERF_CACHE_TYPE(id, op_id, op_result_id) \
  ((id) | ((op_id) << 8) | ((op_result_id) << 16))

static int event_open(enum perf_type_id type, __u64 config, __u64 exclude_kernel, __u64 exclude_hv, __u64 exclude_callchain_kernel, int cpu) {
  static struct perf_event_attr attr;
  memset(&attr, 0, sizeof(attr));
  attr.type = type;
  attr.config = config;
  attr.size = sizeof(attr);
  attr.exclude_kernel = exclude_kernel;
  attr.exclude_hv = exclude_hv;
  attr.exclude_callchain_kernel = exclude_callchain_kernel;
  attr.sample_type = PERF_SAMPLE_IDENTIFIER;
  attr.inherit = 1;
  attr.disabled = 1;

  int fd = syscall(__NR_perf_event_open, &attr, -1, 0, -1, 0);
  assert(fd >= 0 && "perf_event_open failed: you forgot sudo or you have no perf event interface available for the userspace.");

  return fd;
}

static int performance_counter_open(size_t pid, size_t type, size_t config) {
    struct perf_event_attr pe_attr;
    memset(&pe_attr, 0, sizeof(struct perf_event_attr));

    pe_attr.type = type;
    pe_attr.size = sizeof(pe_attr);
    pe_attr.config = config;
    pe_attr.exclude_kernel = 1;
    pe_attr.exclude_hv = 1;
    pe_attr.exclude_callchain_kernel = 1;

    int fd = syscall(__NR_perf_event_open, &pe_attr, pid, -1, -1, 0);
    if (fd == -1) {
        std::throw_with_nested(std::runtime_error("Performance counter monitor could not be opened"));
    }
    assert(fd >= 0);

    return fd;
}

static void performance_counter_reset(int fd) {
  int rc = ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  assert(rc == 0);
}

static void performance_counter_enable(int fd) {
  int rc = ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
  assert(rc == 0);
}

static void performance_counter_disable(int fd) {
  int rc = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
  assert(rc == 0);
}

static size_t performance_counter_read(int fd) {
  size_t count;
  int got = read(fd, &count, sizeof(count));
  assert(got == sizeof(count));

  return count;
}
#endif

