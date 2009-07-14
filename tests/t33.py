def xbin(x): # return integer as string in binary
  if x <= 0:
    return "0"
  else:
    if (x&1 == 0):
      s = "0"
    else:
      s = "1"
    x = x >> 1
    while (x > 0):
      if (x&1 == 0):
        s = "0" + s
      else:
        s = "1" + s
      x = x >> 1
    return s

print xbin(23)
