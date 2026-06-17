# Negative test for the post-rec-1 invariant: an unresolved
# runtime helper now fails LOUDLY at link time rather than
# silently emitting wrong code (the old CG2_PRIM no-op default
# in cg_normalize_v2.cc).  Both backends should fail to compile
# this — the C backend gets `_CG_definitely_not_a_helper_qjm`
# from write_send's `_CG_%s` emit, the v2 LLVM backend gets it
# from the lower_send_prim CG2_C_CALL default — both surface
# as an "undefined reference to ..." linker error.  Marked with
# `.check_fail` so a compile failure counts as a pass.

x = __pyc_c_call__(int, "_CG_definitely_not_a_helper_qjm", int, 42)
print(x)
