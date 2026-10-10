# An except/else/finally belongs to a `try` only if it starts at the try's
# column. The grammar's try_stmt had no such guard (if/while/for did), so
# GLR attached an OUTER statement's `else` across a two-level dedent to a
# `try` nested in its arm, as the try's else clause. rdb's open_log was the
# case: `if options.logging: try: logfile = open(...) except OSError:
# logfile = None` + `else: logfile = None` reset logfile right after a
# successful open, and its log file stayed empty.

def bad(v):
    raise ValueError(v)

def if_try(flag):
    if flag:
        try:
            x = 1
        except ValueError:
            x = 2
    else:
        x = 3
    return x

def for_try(n):
    x = 0
    for i in range(n):
        try:
            x += 1
        except ValueError:
            x = -1
    else:
        x += 100
    return x

def while_try(n):
    x = 0
    while n > 0:
        n -= 1
        try:
            x += 1
        except ValueError:
            x = -1
    else:
        x += 1000
    return x

def all_clauses(flag, v):
    if flag:
        try:
            x = int(v) if v != "q" else bad(v)
        except ValueError:
            x = -1
        else:
            x = x + 100
        finally:
            x = x * 2
    else:
        try:
            x = 5
        finally:
            x = x + 1
    return x

def nested_if_else(flag):
    if flag:
        if not flag:
            x = 1
        else:
            x = 2
    else:
        x = 3
    return x

print(if_try(True), if_try(False))
print(for_try(0), for_try(3))
print(while_try(0), while_try(2))
print(all_clauses(True, "3"), all_clauses(True, "q"), all_clauses(False, "3"))
print(nested_if_else(True), nested_if_else(False))
