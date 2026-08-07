# issue 056: P_prim_index_object/P_prim_set_index_object's new
# scalar_ct(c_type(...)) index-type guard must not false-positive on
# ordinary, correctly-typed indexing -- this exercises the common
# get/set shapes (list, string, tuple-list) the guard sits in front
# of, confirming the fix didn't disturb normal indexing.
# Companion to list_index_type_mismatch_salvage.py (the real
# vendored program that found the bug -- no minimal repro of the
# actual mismatch was ever isolated) and list_element_type_mismatch_salvage.py
# (issue 035's sibling guard on the stored VALUE rather than the index).
a = [10, 20, 30]
print(a[0])
print(a[-1])
a[1] = 99
print(a)

s = "hello"
print(s[1])
print(s[-1])

t = (1, "two", 3.0)
i = 2
print(t[i])
