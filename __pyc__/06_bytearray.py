@vector("s")
class bytearray:
  length = 0
  def __init__(self, s):
    self.length = s
  def __getitem__(self, key):
    if key < 0:
      key = key + self.length
    return __pyc_primitive__(__pyc_symbol__("coerce"), int,
                             __pyc_primitive__(__pyc_symbol__("index_object"), self, key))
  def __setitem__(self, key, value):
    if key < 0:
      key = key + self.length
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self,
                             key,
                             __pyc_primitive__(__pyc_symbol__("coerce"), __pyc_char__, value))
  def __len__(self):
    return self.length
  def __pyc_tobytearray__(self):
    # bytearray(a_bytearray): a copy.
    n = self.length
    r = bytearray(n)
    for i in range(n):
      r[i] = self[i]
    return r
  def __pyc_tobytes__(self):
    # bytes(a_bytearray) (sokoban's `bytes(data2)`).
    r = []
    for i in range(self.length):
      r.append(self[i])
    return r.__pyc_tobytes__()
  def __iter__(self):
    return __base_iter__(self)
  def __str__(self):
    x = "bytearray(b'"
    for k in range(0, len(self)):
      c = self[k]
      if c == 9:
        x += "\\t"
      elif c == 10:
        x += "\\n"
      elif c == 13:
        x += "\\r"
      elif c == 92:
        x += "\\\\"
      elif c == 39:
        x += "\\'"
      elif c >= 32 and c < 127:
        x += chr(c)
      else:
        x += "\\x" + __byte_hex2(c)
    x += "')"
    return x
