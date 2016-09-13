class C:
  def foo(self, x):
    return self.x + x

a = C()
a.x = 1
print a.foo(1)
