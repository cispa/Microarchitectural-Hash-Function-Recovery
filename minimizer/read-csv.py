import sys

if len(sys.argv) < 3:
    print("./read-csv.py <path to folder with CSVs> <bit number> [negate]")
    exit(1)

path = sys.argv[1]
bit_num = int(sys.argv[2])
negate = 0 if len(sys.argv) == 4 and sys.argv[3] == "negate" else 1

with open(f"{path}/bit_{bit_num}.csv") as f:
    data = f.readlines()

with open(f"{path}/bits.csv") as f:
    bits_data = f.readlines()

relevant_bits = [int(x.strip()) for x in bits_data[bit_num].split(",") if x.strip() != '']
irrelevant_bits = set(range(46)) - set(relevant_bits)

n = len(relevant_bits)

outfile = f"{path}/bit_{bit_num}.espresso.in"

seen = set()

with open(outfile, "w") as f:

    f.write(f".i {n}\n")
    f.write(f".o 1\n")

    for line in data[1:]:
        x,y = line.split(",")

        x = int(x)
        y = int(y)

        bin_repr = ""

        for bits in relevant_bits:
            bin_repr += str((x >> (bits)) & 1)


        bit_val = int(bin_repr, 2)

        seen.add(bit_val)

        if y == negate:
            f.write(f"{bin_repr} {1}\n")
        else:
            pass

    f.write(".e\n")


print(f"Output written to {outfile}/")
print(f"espresso {outfile} > {path}/bit_{bit_num}.espresso.sol")
print(f"sage minimize-groebner.sage {path}/bit_{bit_num}.espresso.sol")

assert len(seen) == 2**n, f"{len(seen)=} {n=}"
