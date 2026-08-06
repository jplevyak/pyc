# issue 035 follow-up: __dict_iter__/__dict_items_iter__/__set_iter__
# are shared program-wide -- every dict's .keys()/.values()/.items()
# (and every set's iteration) constructs one, so their fields are
# inherently the union of every CALLING dict/set's key/value/element
# type unless something splits per-receiver CreationSets apart.
# Mirrors __list_iter__/range's existing __pyc_clone_constants__ lever
# (issue 045) instead of removing the class-body defaults the way
# issue 076 fixed dict/set THEMSELVES -- that mechanical mirroring
# was tried first and broke this exact scenario (an int-keyed dict
# and a str-keyed dict both calling .keys() in the same program hit a
# hard "_CG_str_eq(..., _CG_any)" compile error), found via
# shedskin_examples/webserver/webserver.py
# (self.mapSocks.keys() int-keyed, headers.keys() str-keyed).
#
# Deliberately wrapped in a function, not bare module-level code:
# the clone_methods_per_cs per-receiver-CS split this fix relies on
# needs a per-call-site contour to split BY -- bare top-level/
# __main__ code doesn't get the same per-invocation specialization
# ordinary function bodies do, so the identical dict.keys() calls at
# module level still reproduce this bug (a separate, narrower,
# pre-existing limitation, not what caused webserver.py's regression
# -- its code is entirely inside methods, matched here).
def f():
    int_keyed = {1: "one", 2: "two"}
    str_keyed = {"a": 1, "b": 2}

    for k in int_keyed.keys():
        print(k, int_keyed[k])

    for k in str_keyed.keys():
        print(k, str_keyed[k])

    print(1 in int_keyed.keys())
    print("a" in str_keyed.keys())

    for k, v in str_keyed.items():
        print(k, v)

f()
