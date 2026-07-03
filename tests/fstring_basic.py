x = 5
print(f"value is {x}")
print(f"{{literal}}")
y = 3
print(f"{y + 1}")
a = 1
b = 2
print(f"a={a} b={b} sum={a+b}")
print(f"")
print(f"no interp here")
s = "hi"
print(f"{s!r}")
print(f"{s!s}")
d = {"a": 1}
print(f"val={d['a']}")


class Foo:
    def bar(self):
        return 42


foo = Foo()
print(f"result: {foo.bar()}")
print(f"""triple: {x}""")
