import sys
import os

path = sys.argv[1]

with open(f"{path}/bits.csv") as f:
    dat = f.readlines()

num_bits = len(dat)

for bit in range(num_bits):
    print("=====================")
    print(f"Starting bit {bit}")
    print("=====================")

    os.system(f"/usr/bin/python3 read-csv.py {path} {bit}")
    os.system(f"/usr/bin/python3 test.py {path}/bit_{bit}.espresso.in")