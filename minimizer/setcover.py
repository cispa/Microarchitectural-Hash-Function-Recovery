from pulp import *

def set_cover(subsets, weights=None):
    if weights is None:
        weights = [1] * len(subsets)

    label = dict()
    new_subsets = []
    for subset in subsets:
        new_subset = set()
        for element in subset:
            if element not in label:
                label[element] = len(label)
            new_subset.add(label[element])
        new_subsets.append(new_subset)
    subsets = new_subsets

    n = len(subsets)
    m = len(label)

    problem = LpProblem("set-cover", LpMinimize)

    x = [LpVariable(f"x{i}", 0, 1, LpInteger) for i in range(n)]

    problem += lpSum(weights[i] * x[i] for i in range(n))

    for i in range(m):
        problem += lpSum(x[j] for j in range(n) if i in subsets[j]) >= 1

    solver = GLPK_CMD(msg=False)
    problem.solve(solver)

    solution = [x[i].value() > 0.5 for i in range(n)]

    return solution

if __name__ == "__main__":
    subsets = [
        [1, 2, 4, 8, 9],
        [1, 3, 4],
        [2, 4, 7, 8],
        [2, 7, 9]
    ]
    weights = [1, 1, 2, 1]
    print(set_cover(subsets, weights))