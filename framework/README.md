# Unscatter Measurement Framework
Unscatter is the measurement framework for microarchitectural hash functions.
It allows dumping measurement results that can later be minimized using a logic minimization algorithm.

# Running Unscatter
Currently, only tested on Ubuntu 22.04.
## Dependencies
- Pteditor
- Boost
## Building
```bash
mkdir build
cd build
cmake ..
make -j
```
## Running 
For example to run the cache slice measurement on the isolated core 0 of an Intel Core processor run.
```bash
sudo ./unscatter -c 0 -s 0 
```
## Options
```
Unscatter framework.
Usage: ./unscatter [OPTIONS]

Options:
  -h,--help                     Print this help message and exit
  -c,--core INT                 Core to pin framwork to
  -s,--set-option INT           Set currently implemented frameworks 0=Slice Direct, 1=Slice Indirect, 2=Utag Indirect, 3=DRAM Indirect
  -o,--output-classes INT       Number of output classes, (if not given then determined automatically)
  -t,--tresh-oracle INT         Treshold of oracle in percent default 90%
  -u,--bit-limit-upper INT      Bit limit for highest bit that gets dumped
  -l,--bit-limit-lower INT      Bit limit for lowest bit that gets dumped
  -r,--relevant-input-bits TEXT File containing the relevant input bits
  --dry-run                     Perform a dry run without measurements.
  --xeon                        Enable if tested processor is an Intel Xeon chip
```
