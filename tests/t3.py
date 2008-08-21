def hi(a, b, c):
  if a and b and c: 
    print 'and true'
  else:
    print 'and false'
  if a or b or c: 
    print 'or true'
  else:
    print 'or false'
hi(True, True, True)
print 'and true *'
print 'or true *'
hi(False, False, False)
print 'and false *'
print 'or false *'
hi(True, False, True)
print 'and false *'
print 'or true *'
hi(False, True, False)
print 'and false *'
print 'or true *'
hi(True, True, False)
hi(False, False, True)
print 'and false *'
print 'or true *'
hi(True, False, False)
hi(False, True, True)
print 'and false *'
print 'or true *'
