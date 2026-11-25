# pyc shim for functools. reduce uses the default-None + is-None
# narrowing pattern (see __pyc__/05_builtins.py min/max) instead of
# CPython's private sentinel class -- a class-object default is
# untypable for pyc today. reduce(f, seq, None) as an EXPLICIT
# initializer is therefore indistinguishable from the 2-arg form
# (nobody in the corpus does that).

def reduce(function, iterable, initializer=None):
    it = iter(iterable)
    if initializer is None:
        value = next(it)
    else:
        value = initializer
    for element in it:
        value = function(value, element)
    return value


# lru_cache and cache (issues/207): a wrapper object whose __call__ looks up
# the positional arguments, as a tuple, in a dict. Keyword arguments to the
# cached function are not supported (a compile error: __call__ takes none).
# `typed=True` is accepted but changes nothing, which matters only for
# arguments that compare equal across types (1 and 1.0), refused below.
class _CacheInfo:
    def __init__(self, hits, misses, maxsize, currsize):
        self.hits = hits
        self.misses = misses
        self.maxsize = maxsize
        self.currsize = currsize

    def __repr__(self):
        return ("CacheInfo(hits=" + str(self.hits) + ", misses=" + str(self.misses) +
                ", maxsize=" + str(self.maxsize) + ", currsize=" + str(self.currsize) + ")")

    def __eq__(self, other):
        return (self.hits == other.hits and self.misses == other.misses and
                self.maxsize == other.maxsize and self.currsize == other.currsize)


class _lru_cache_wrapper:
    def __init__(self, func, maxsize):
        self.__wrapped__ = func
        self._maxsize = maxsize
        self._cache = {}
        self._hits = 0
        self._misses = 0

    def __call__(self, *args):
        cache = self._cache
        if args in cache:
            self._hits += 1
            v = cache[args]
            if self._maxsize is not None:
                # most recently used last: CPython's eviction order
                del cache[args]
                cache[args] = v
            return v
        self._misses += 1
        v = self.__wrapped__(*args)
        if self._maxsize is None:
            cache[args] = v
        elif self._maxsize > 0:
            if args in cache:
                # a recursive call filled it meanwhile
                return v
            if len(cache) >= self._maxsize:
                for oldest in cache:
                    del cache[oldest]
                    break
            cache[args] = v
        return v

    def cache_info(self):
        return _CacheInfo(self._hits, self._misses, self._maxsize, len(self._cache))

    def cache_clear(self):
        self._cache.clear()
        self._hits = 0
        self._misses = 0


class _lru_cache_decorator:
    def __init__(self, maxsize):
        self._maxsize = maxsize

    def __call__(self, func):
        return _lru_cache_wrapper(func, self._maxsize)


# @lru_cache, @lru_cache(), @lru_cache(maxsize=N) and @lru_cache(None).
def lru_cache(maxsize=128, typed=False):
    if isinstance(maxsize, int):
        if maxsize < 0:
            maxsize = 0
        return _lru_cache_decorator(maxsize)
    if maxsize is None:
        return _lru_cache_decorator(None)
    # used bare: maxsize is the function
    return _lru_cache_wrapper(maxsize, 128)


def cache(user_function):
    return _lru_cache_wrapper(user_function, None)


# functools.wraps / update_wrapper: CPython copies __name__, __doc__,
# __module__, __qualname__, __dict__ and sets __wrapped__. pyc functions
# have none of those attributes to read (a read of one is a compile error),
# so the wrapper is returned unchanged.
class _identity_decorator:
    def __call__(self, wrapper):
        return wrapper


def wraps(wrapped, assigned=None, updated=None):
    return _identity_decorator()


def update_wrapper(wrapper, wrapped, assigned=None, updated=None):
    return wrapper


# cmp_to_key: CPython's K class, comparing through the old-style cmp.
class _CmpKey:
    def __init__(self, cmp, obj):
        self.cmp = cmp
        self.obj = obj

    def __lt__(self, other):
        return self.cmp(self.obj, other.obj) < 0

    def __gt__(self, other):
        return self.cmp(self.obj, other.obj) > 0

    def __eq__(self, other):
        return self.cmp(self.obj, other.obj) == 0

    def __le__(self, other):
        return self.cmp(self.obj, other.obj) <= 0

    def __ge__(self, other):
        return self.cmp(self.obj, other.obj) >= 0


class _CmpToKey:
    def __init__(self, cmp):
        self.cmp = cmp

    def __call__(self, obj):
        return _CmpKey(self.cmp, obj)


def cmp_to_key(mycmp):
    return _CmpToKey(mycmp)


# partial: CPython's semantics for positional arguments. The stored
# arguments go before the call's through tuple.__pyc_call_prepended__, one
# overload per arity of the stored tuple (python_ifa_main.cc), so they bind
# the function's named parameters with their own types. Keyword arguments,
# stored or given at the call, need `**` into named parameters, which is
# issues/189 step 2: the frontend refuses a partial built with keywords
# (python_ifa_dataclass.cc), and `keywords` is always empty.
class partial:
    def __init__(self, func, *args):
        self.func = func
        self.args = args
        self.keywords = {}

    def __call__(self, *args):
        return self.args.__pyc_call_prepended__(self.func, args)


# total_ordering is lowered by the frontend (python_ifa_dataclass.cc): a
# module-level class it decorates gets CPython's generated comparison
# methods, and any other use of the name is refused there. This definition
# only gives the import a name to bind.
def total_ordering(cls):
    return cls
