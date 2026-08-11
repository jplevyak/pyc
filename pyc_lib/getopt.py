# pyc shim for the standard `getopt` module (issues/041). Was a no-op
# stub -- getopt()/gnu_getopt() always returned ([], []) regardless of
# args, so any program parsing real CLI flags this way silently
# ignored them and fell through to defaults with no diagnostic.
#
# Real short/long-option parsing, ported from CPython's own
# Lib/getopt.py algorithm (simplified for pyc's Python subset: no
# isinstance(longopts, str) single-string convenience form -- callers
# always pass a list; uses str.find() instead of str.index() to avoid
# exception-based control flow for a plain substring search).
#
# ifa/issues/095: on the LLVM backend specifically (-b), a long option
# with no argument (optarg genuinely None on that branch) can read back
# wrong -- confirmed a str|None-typed local misbehaving on -b, C
# backend unaffected. Verified correct on the default C backend.

class GetoptError(Exception):
    def __init__(self, msg, opt=""):
        self.msg = msg
        self.opt = opt
        Exception.__init__(self, msg)
    def __str__(self):
        return self.msg

error = GetoptError


def _short_has_arg(opt, shortopts):
    i = shortopts.find(opt)
    if i < 0:
        raise GetoptError("option -" + opt + " not recognized", opt)
    return i + 1 < len(shortopts) and shortopts[i + 1] == ':'


def _do_shorts(opts, optstring, shortopts, args):
    while optstring != "":
        opt = optstring[0]
        optstring = optstring[1:]
        if _short_has_arg(opt, shortopts):
            if optstring == "":
                if not args:
                    raise GetoptError("option -" + opt + " requires argument", opt)
                optstring = args[0]
                args = args[1:]
            optarg = optstring
            optstring = ""
        else:
            optarg = ""
        opts.append(("-" + opt, optarg))
    return opts, args


def _match_long_opt(opt, longopts):
    possibilities = []
    for o in longopts:
        if o.startswith(opt):
            possibilities.append(o)
    if not possibilities:
        raise GetoptError("option --" + opt + " not recognized", opt)
    if opt in possibilities:
        exact = opt
    elif (opt + "=") in possibilities:
        exact = opt + "="
    elif len(possibilities) == 1:
        exact = possibilities[0]
    else:
        raise GetoptError("option --" + opt + " not a unique prefix", opt)
    if exact.endswith("="):
        return True, exact[:-1]
    return False, exact


def _do_longs(opts, opt, longopts, args):
    eq = opt.find("=")
    if eq < 0:
        optarg = None
    else:
        optarg = opt[eq + 1:]
        opt = opt[:eq]
    has_arg, opt = _match_long_opt(opt, longopts)
    if has_arg:
        if optarg is None:
            if not args:
                raise GetoptError("option --" + opt + " requires argument", opt)
            optarg = args[0]
            args = args[1:]
    elif optarg is not None:
        raise GetoptError("option --" + opt + " must not have an argument", opt)
    else:
        optarg = ""
    opts.append(("--" + opt, optarg))
    return opts, args


def getopt(args, shortopts, longopts=[]):
    opts = []
    while args and args[0] != "-" and args[0].startswith("-"):
        if args[0] == "--":
            args = args[1:]
            break
        if args[0].startswith("--"):
            opts, args = _do_longs(opts, args[0][2:], longopts, args[1:])
        else:
            opts, args = _do_shorts(opts, args[0][1:], shortopts, args[1:])
    return opts, args


def gnu_getopt(args, shortopts, longopts=[]):
    opts = []
    prog_args = []
    all_options_first = False
    if shortopts.startswith("+"):
        shortopts = shortopts[1:]
        all_options_first = True

    while args:
        if args[0] == "--":
            prog_args = prog_args + args[1:]
            break
        if args[0].startswith("--"):
            opts, args = _do_longs(opts, args[0][2:], longopts, args[1:])
        elif args[0] != "-" and args[0].startswith("-"):
            opts, args = _do_shorts(opts, args[0][1:], shortopts, args[1:])
        else:
            if all_options_first:
                prog_args = prog_args + args
                break
            prog_args.append(args[0])
            args = args[1:]

    return opts, prog_args
