# ifa/issues/053: unpacking `for rule, weight in grammar` where
# `grammar`'s first-position element (`rule`) has genuinely varying
# arity across entries (grammar rules with a variable RHS length,
# shedskin's plcfrs.py) segfaulted the C backend only. Root cause:
# the constant-field-getter in ifa/codegen/cg.cc's P_prim_index_object
# rejected a field whose own type is itself a Type_SUM (union) with
# ->size == 0 -- even though every member of that union happens to be
# uniformly pointer-sized -- silently skipping the getter emission and
# leaving the destination Var declared but never assigned (read as
# uninitialized stack garbage downstream). Fixed by extending the
# existing sizeof_element uniform-size check (resolve_uniform_size())
# to the getter too. This is the exact repro from the issue file,
# distinct from tuple_arity_union.py (which covers the earlier,
# shallower tuple-list-header bug: direct iteration/indexing of a
# heterogeneous-arity tuple union, not unpacking one out of a wrapper).
grammar = [(("S", "NP", "VP"), -0.1), (("NP", "N"), -0.2)]
result = [rule for rule, weight in grammar]
print(result)
