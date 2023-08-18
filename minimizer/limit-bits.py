import sys

path = sys.argv[1]
bit_num = int(sys.argv[2])

with open(path) as f:
    data = f.readlines()

with open(path, "w") as f:
    for line in data:
        if line.startswith(".i"):
            i = int(line.split(" ")[1]) - bit_num
            f.write(f".i {i}\n")
        elif line.startswith("."):
            f.write(line)
        elif "0" * bit_num + " 1" in line:
            f.write(line.replace("0" * bit_num + " 1", " 1"))