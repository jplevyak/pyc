# ifa/124: a list literal WITH elements that is later appended to. clone.cc
# gives its group a copy of the builtin `list` type sym before codegen names
# the builtins, so the copy had no C name and fell to `_CG_any`; the list
# then read as untyped and --refuse-imprecise (this test's .flags) refused a
# program whose types are all known. The copy now takes `_CG_list`.
xs = [1, 2]
xs.append(3)
print(len(xs), xs[2])
