# Regression: comparing string constants must not crash the constant
# folder. fold_constant tried to numerically coerce a string operand
# and aborted coerce_immediate on the unhandled cast (issue 025 bucket
# E, sudoku2). Now it bails to a runtime comparison.
def classify(s):
    if s == "yes":
        return 1
    elif s == "no":
        return 0
    else:
        return -1

def main():
    print(classify("yes"))
    print(classify("no"))
    print(classify("maybe"))
    print("a" == "a")
    print("a" == "b")

main()
