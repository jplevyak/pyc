from pyc_compat import __pyc_declare__
class C:
  value = __pyc_declare__
  def __init__(self, val):
    self.value = val

c1 = C(1)
c2 = C("2")
c3 = C(1.1);
c4 = C(C(C(C(c3))))
c3.value = 2.5

print c1.value
print c2.value
print c3.value
print c4.value.value.value.value.value
