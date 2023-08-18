class BinaryOperation:

    def __init__(self, symbol, func, children):
        self.symbol = symbol
        self.func = func
        self.children = children

    def __str__(self):
        str_children = [str(child) for child in self.children]
        if len(str_children) == 1:
            return str_children[0]
        else:
            return "(" + (" " + self.symbol + " ").join(str_children) + ")"

    def eval(self, assignment):
        return self.func(child.eval(assignment) for child in self.children)


class And(BinaryOperation):

    def __init__(self, *children):
        BinaryOperation.__init__(self, "&", all, children)


class Or(BinaryOperation):

    def __init__(self, *children):
        BinaryOperation.__init__(self, "|", any, children)


class Xor(BinaryOperation):

    def __init__(self, *children):
        func = lambda x: sum(x) % 2 == 1
        BinaryOperation.__init__(self, "^", func, children)


class Not:

    def __init__(self, child):
        self.child = child

    def __str__(self):
        return "~" + str(self.child)

    def eval(self, assignment):
        return not self.child.eval(assignment)


class Var:

    def __init__(self, number):
        self.number = number

    def __str__(self):
        return f"x{self.number}"

    def eval(self, assignment):
        return assignment[self.number]


f = Or(And(Not(Var(1)), Not(Var(0)), Var(3)), And(Not(Var(1)), Not(Var(0)), Var(2)), Not(Var(4)))

print(f)
print(f.eval([0, 1, 0, 1, 1]))
    