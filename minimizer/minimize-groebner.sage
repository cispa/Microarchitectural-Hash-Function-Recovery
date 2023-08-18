from sage.all import *
import itertools as it
import sys

if len(sys.argv) < 2:
    print("sage minimize-groebner.sage <espresso output>")
    exit(1)

NEGATE = 0

with open(sys.argv[1]) as f:
    lines = f.readlines()

n = [int(line.split(" ")[1]) for line in lines if line[:2] == ".i"][0]


R = PolynomialRing(GF(2), "x", n)
x = R.gens()
Q = R

relations = []

lines_preproc = []

# Filter out all bits that are always constant
bit_status = [0] * n
for line in lines:
    if line[0] == ".":
        continue


    var, res = line.split(" ")
    assert len(var) == n

    for (i, val) in enumerate(var):
        bit_var = {
            "0": 0b001,
            "1": 0b010,
            "-": 0b100,
        }[var[i]]
        bit_status[i] |= bit_var

for i in range(n):
    if not (bit_status[i] & (bit_status[i] - 1)):
        print(f"Bit {i} is irrelevant!")


for line in lines:

    if line[0] == ".":
        continue

    var, res = line.split(" ")


    term = 1

    for i in range(len(var)):
        if not (bit_status[i] & (bit_status[i] - 1)):
            continue
        if var[i] == "-":
            continue
        elif var[i] == '1':
            term *= x[i] + NEGATE
        elif var[i] == '0':
            term *= x[i] + (1-NEGATE)


    relations.append(term)

idempotency_relations = [x[i]**2 + x[i] for i in range(n)]

for i in range(n):
    relations.append(idempotency_relations[i])


def groebner_core(relations):
    global n
    global Q

    if len(relations) == 0:
        print("This formula is always false.")
        exit(1)
    if len(relations) == 2**n:
        print("This formula is a tautology")

    print("Sampling completed.")

    I = Q.ideal(relations)
    print("Starting Groebner.")

    B = I.groebner_basis()
    print("Done Groebner.")
    return B

B = groebner_core(relations)

def is_trivial_elem(b):
    return b in idempotency_relations

B = [b for b in B if not is_trivial_elem(b)]

print("==========================================")

for b in B:
    fB = factor(b)

    outfun = ""
    terms = []
    for (term,_) in fB:
        monomials = []
        for monom in term.monomials():
            variables = []
            if monom == 1:
                variables.append("1")
            for var in monom.variables():
                variables.append(f"x[{x.index(var)}]")
            monomials.append("" + "&".join(variables) + "")
        terms.append("(" + " ^ ".join(monomials) + ")")
    outfun = " & ".join(terms)
    print(outfun)


print(f"===== {len(B)} element in Groebner Basis =====")
