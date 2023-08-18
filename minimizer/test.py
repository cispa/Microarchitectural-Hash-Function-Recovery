from sage.all import *

import itertools as it
import sys
import subprocess
import setcover
import time
import os



ESPRESSO_EXECUTABLE = os.environ.get("ESPRESSO_EXECUTABLE") # Path to the espresso executable

_TIME_TOTAL = -time.time()
_TIME_IN_ESPRESSO = 0
_TIME_IN_GROEBNER = 0
_TIME_IN_SETCOVER = 0
_TOTAL_GROEBNER_CALLS = 0

def simplify_not_not(node):
    if not isinstance(node, UnaryNode):
        return node
    if not isinstance(node.child, UnaryNode):
        return node
    if node.symbol == "~" and node.child.symbol == "~":
        return node.child.child
    else:
        return node

def simplify_binary_one_child(node):
    if not isinstance(node, BinaryNode):
        return node
    if len(node.children) == 1:
        return node.children[0]
    else:
        return node

def simplify_binary_same_child(node):
    if not isinstance(node, BinaryNode):
        return node
    children = []
    for child in node.children:
        if isinstance(child, BinaryNode) and child.symbol == node.symbol:
            children.extend(child.children)
        else:
            children.append(child)
    return BinaryNode(node.symbol, children)

def simplify_all(node):
    node = simplify_not_not(node)
    node = simplify_binary_one_child(node)
    node = simplify_binary_same_child(node)
    return node

def modify(node, func):
    if isinstance(node, BinaryNode):
        node.children = [modify(child, func) for child in node.children]
    if isinstance(node, UnaryNode):
        node.child = modify(node.child, func)
    return func(node)

class BinaryNode:

    def __init__(self, symbol, children):
        self.symbol = symbol
        self.children = children
    def __repr__(self):
        return f"BinaryNode(\"{self.symbol}\", {repr(self.children)})"

    def __str__(self):
        str_children = [str(child) for child in self.children]
        if len(str_children) == 1:
            return str_children[0]
        else:
            return "(" + (" " + self.symbol + " ").join(str_children) + ")"

    def copy(self):
        return BinaryNode(self.symbol, [child.copy() for child in self.children])

    def size(self):
        return 1 + sum(child.size() for child in self.children)

    def find(self, symbol):
        if self.symbol == symbol:
            yield self
        else:
            for child in self.children:
                yield from child.find(symbol)

    def leafs(self):
        for child in self.children:
            yield from child.leafs()

    def traverse(self, depth=0):
        global _TIME_IN_SETCOVER
        print("-" * depth, self.symbol)
        if self.symbol != "^":
            for child in self.children:
                child.traverse(depth + 1)
        if self.symbol in ["|", "&"]:
            subsets = []
            weights = []
            for child in self.children:
                weights.append(child.size())
                formula = eval("lambda x: " + str(child) + " & 1")
                if self.symbol == "|":
                    subset = [value for value in it.product([0, 1], repeat=n) if formula(value)] 
                else:
                    subset = [value for value in it.product([0, 1], repeat=n) if not formula(value)] 
                subsets.append(subset)
            time_start = time.time()
            cover = setcover.set_cover(subsets, weights)
            _TIME_IN_SETCOVER += time.time() - time_start
            self.children = [child for i, child in enumerate(self.children) if cover[i]]


class UnaryNode:

    def __init__(self, symbol, child):
        self.symbol = symbol
        self.child = child

    def __repr__(self):
        return f"UnaryNode(\"{self.symbol}\", {repr(self.child)})"

    def __str__(self):
        return self.symbol + str(self.child)

    def copy(self):
        return UnaryNode(self.symbol, self.child.copy())

    def size(self):
        return 1 + self.child.size()

    def find(self, symbol):
        if self.symbol == symbol:
            yield self
        else:
            yield from self.child.find(symbol)

    def leafs(self):
        yield from self.child.leafs()

    def traverse(self, depth=0):
        print("-" * depth, self.symbol)
        self.child.traverse(depth + 1)

class LeafNode:

    def __init__(self, value):
        self.value = value

    def __repr__(self):
        return f"LeafNode({repr(self.value)})"

    def __str__(self):
        return str(self.value)

    def copy(self):
        return LeafNode(self.value)

    def size(self):
        return 1

    def leafs(self):
        yield self

    def traverse(self, depth=0):
        print("-" * depth, self.value)

    def find(self, symbol):
        yield from []


def And(children):
    return BinaryNode("&", children)

def Or(children):
    return BinaryNode("|", children)

def Xor(children):
    return BinaryNode("^", children)

def Not(child):
    return UnaryNode("~", child)

def Value(value):
    return LeafNode(value)

def parse_polynomial(polynomial):
    negate = False
    monomials = []
    for monomial in polynomial.monomials():
        if monomial == 1:
            negate = True
            continue
        variables = [Value(var) for var, _ in factor(monomial)]
        monomial = And(variables)
        monomials.append(monomial)
    polynomial = Xor(monomials)
    if negate:
        polynomial = Not(polynomial)
    return polynomial


def sample_polynomial(p):
    var = p.variables()
    n = len(var)
    values = []
    for i in range(1 << n):
        bitstring = [(i >> j) & 1 for j in range(n)]
        assignment = dict(zip(var, bitstring))
        value = p.subs(assignment)
        if value == 1:
            values.append(bitstring)
    return var, values

def read_bits_csv(path, bit_num):
    with open(f"{path}/bit_{bit_num}.csv") as f:
        data = f.readlines()

    with open(f"{path}/bits.csv") as f:
        bits_data = f.readlines()

    relevant_bits = [int(x.strip()) for x in bits_data[bit_num].split(",") if x.strip() != '']
    n = len(relevant_bits)

    values = []
    for line in data:
        x, y = line.split(",")
        x = int(x)
        y = int(y)

        bin_repr = []
        for bits in relevant_bits:
            bin_repr.append((x >> bits) & 1)

        values.append((tuple(bin_repr), y))

    return relevant_bits, values

def read_espresso_in(path):
    with open(f"{path}") as f:
        data = f.readlines()

    values = []
    seen_inputs = set()
    n = -1
    for line in data:
        line = line.strip()
        if line[0] == ".":
            continue

        x, y = line.split(" ")
        y = int(y)

        assert n == -1 or n == len(x)
        n = len(x)

        bin_repr = []
        for bit in x:
            assert bit == "1" or bit == "0", "found wildcard, this does not work."
            bin_repr.append(int(bit))

        values.append((tuple(bin_repr), y))
        seen_inputs.add(tuple(bin_repr))

    for bits in it.product([0, 1], repeat=n):
        if bits not in seen_inputs:
            values.append((bits, 0))


    return n, values

def read_blackbox():
    n = 6
    blackbox = lambda x: x[1] & ((x[0] ^ x[5]) & (x[2]) | (x[3] & (x[4] ^ x[5])))
    values = []
    for bits in it.product([0, 1], repeat=n):
        values.append((bits, blackbox(bits)))
    return (n, values)


def compute_groebner_basis(bits, values):
    global _TIME_IN_ESPRESSO, _TIME_IN_GROEBNER, _TOTAL_GROEBNER_CALLS

    _TOTAL_GROEBNER_CALLS += 1

    n = len(bits)
    print(f"Starting Espresso ({n} bits, {len(values)} truth-values).")
    espresso = subprocess.Popen(ESPRESSO_EXECUTABLE, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    write = lambda line: espresso.stdin.write(line.encode())
    write(f".i {n}\n")
    write(f".o 1\n")
    for bitstring in values:
        bin_repr = "".join(map(str, bitstring))
        write(f"{bin_repr} {1}\n")
    write(".e\n")


    time_start = time.time()
    out, err = espresso.communicate()
    time_end = time.time()

    _TIME_IN_ESPRESSO += (time_end - time_start)

    if err:
        print("Error:", err.decode())
        sys.exit(1)

    relations = []

    for line in out.decode().split("\n"):
        if not line or line.startswith("."):
            continue
        var, res = line.split(" ")
        term = 1
        for i in range(len(var)):
            if var[i] == "-":
                continue
            elif var[i] == '1':
                term *= bits[i]
            elif var[i] == '0':
                term *= bits[i] + 1
        relations.append(term)

    idempotency_relations = [bits[i]**2 + bits[i] for i in range(n)]
    relations.extend(idempotency_relations)


    I = R.ideal(relations)

    print(f"Starting Groebner ({len(relations)} relations).")
    time_start = time.time()

    B = I.groebner_basis()
    B = [b for b in B if b not in idempotency_relations]
    time_end = time.time()
    _TIME_IN_GROEBNER += (time_end - time_start)

    return B

seen = set()

def minimize_rec(x, bitstrings, negate=False, depth=0, size=10**10):
    basis = compute_groebner_basis(x, bitstrings)
    newsize = sum( len(list(str(b))) for b in basis )
    if newsize > size:
        return None
    result = []
    for element in basis:
        factors = []
        for fac, _ in factor(element):
            if fac.degree() <= 1 or len(list(fac)) <= 1 or fac in seen or depth > 5:

                if negate:
                    factors.append(Not(parse_polynomial(fac)))
                else:
                    factors.append(parse_polynomial(fac))
            else:
                new_bits, new_values = sample_polynomial(fac + 1)
                min_fac = minimize_rec(new_bits, new_values, not negate, depth + 1, newsize)
                seen.add(fac)

                if min_fac is None:
                    if negate:
                        factors.append(Not(parse_polynomial(fac)))
                    else:
                        factors.append(parse_polynomial(fac))
                else:
                    factors.append(min_fac)
        if negate:
            result.append(Or(factors))
        else:
            result.append(And(factors))
    if negate:
        return And(result)
    else:
        return Or(result)

import sys

if len(sys.argv) < 2:
    print("python test.py <path to espresso input> [negate]")
    exit(1)

path = sys.argv[1] 
negate = sys.argv[2] == "negate" if len(sys.argv) > 2 else False


n, data = read_espresso_in(path)


check_value = 1 if not negate else 0
values = [ value for value, y in data if y == check_value ]

R = PolynomialRing(GF(2), "x", n)
x = R.gens()

tree = minimize_rec(x, values, negate=negate)
tree = modify(tree, simplify_all)
tree = modify(tree, simplify_all)

for leaf in tree.leafs():
    leaf.value = x.index(leaf.value)

for leaf in tree.leafs():
    leaf.value = f"x[{leaf.value}]"

_TIME_TREE_SIMPLIFICATION = -time.time()
tree.traverse()
_TIME_TREE_SIMPLIFICATION += time.time()


extract_xors = False

print("Generating Code.")
if not extract_xors:
    code = "formula = lambda x: " + str(tree) + " & 1\n"
    tree_size = tree.size()
else:
    tmp_tree = tree.copy()
    tree_size = 0
    code = "def formula(x):\n"
    remap = {}
    for node in tmp_tree.find("^"):
        str_node = str(node)
        if str_node in remap:
            idx = remap[str_node]
        else:
            idx = remap[str_node] = len(remap)
            code += f"    y{idx} = " + str_node + "\n"
            tree_size += node.size()
        node.children = [LeafNode(f"y{idx}")]
    code += f"    return " + str(tmp_tree) + " & 1\n"
    tmp_tree = modify(tmp_tree, simplify_all)
    tree_size += tmp_tree.size()

print()
print(code)
print()

exec(code)

print("Testing.")
for value, y in data:
    if formula(value) != y:
        print(f"Error: formula({value}) != {y}")
        sys.exit(1)

import os
input_path = sys.argv[1]
dir = os.path.dirname(input_path)
file = os.path.basename(input_path)
file_ending = ".neg.sol" if negate else ".sol"
file = file.split(".")[0] + file_ending

_TIME_TOTAL += time.time()

with open(f"{dir}/{file}", "w") as f:
    f.write(code)

    print("\nTiming: ")
    print(f"{_TIME_IN_ESPRESSO=:.3f}")
    print(f"{_TIME_IN_GROEBNER=:.3f}")
    print(f"{_TIME_IN_SETCOVER=:.3f}")
    print(f"{_TIME_TREE_SIMPLIFICATION=:.3f}")
    print(f"{_TIME_TOTAL=:.3f}")

    f.write("Timing: \n")
    f.write(f"{_TIME_IN_ESPRESSO=:.3f}\n")
    f.write(f"{_TIME_IN_GROEBNER=:.3f}\n")
    f.write(f"{_TIME_IN_SETCOVER=:.3f}\n")
    f.write(f"{_TIME_TREE_SIMPLIFICATION=:.3f}\n")
    f.write(f"{_TIME_TOTAL=:.3f}\n")

    f.write("\nStats: \n")
    f.write(f"{_TOTAL_GROEBNER_CALLS=}\n")
    f.write(f"{n=}\n")
    f.write(f"{tree_size=}\n")


print("Done.")
