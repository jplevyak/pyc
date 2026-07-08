# Regression: a docstring'd function followed by a deeper-nested
# docstring'd method failed to PARSE. The triple-string grammar let
# the body swallow quote characters (longstringitem ::= [^\\]), so
# two triple-quoted strings could also scan as ONE giant string
# spanning the code between them; GLR carried the ambiguity and this
# nesting shape made the bogus scan win (issue 025 bucket D:
# timsort, rdb, solitaire, plcfrs, astar, neural1).
def f():
    """doc for f"""
    return 1

class C:
    def m(self):
        '''doc with "quotes" and it's fine'''
        return 40

def main():
    c = C()
    print(f() + c.m())   # 41
    x = """say "hi" """
    y = '''and 'bye' too'''
    print(len(x) + len(y))

main()
