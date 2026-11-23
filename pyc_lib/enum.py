# pyc shim for the standard `enum` module (issues/208).
#
# A class whose base is Enum (named through this module's imports, or an enum
# class defined earlier in the same module) is lowered by the frontend
# (python_ifa_enum.cc) into the class CPython's EnumType builds:
#
#   - each member assignment becomes an instance of the class, made with
#     __pyc_new_instance__ (calling the class is the value lookup), with
#     `_name_` and `_value_` set and the user's __init__ called on the value,
#     and added in definition order by __pyc_enum_add__ below, which makes a
#     member whose value equals an earlier one's an ALIAS of it;
#   - auto() is the value _generate_next_value_ gives it;
#   - @unique is applied after the members exist.
#
# What CPython's metaclass does on the CLASS (iterate it, len, `in`, index it
# by name, call it with a value) is here as @__pyc_metamethod__ methods: they
# are methods of the class's meta type, so `for m in Color` and `Color(1)`
# dispatch to them, and every enum class inherits them.
#
# Not here, and refused: IntEnum / StrEnum (a member would be an int or str
# carrying a name, and pyc has no such subclass), Flag / IntFlag, the
# functional API
# (`Enum('Color', 'RED GREEN')`), a user __new__ in an enum class, and
# _ignore_.


class auto:
    # Consumed by the lowering, which replaces `NAME = auto()`.
    def __init__(self):
        pass


class Enum:
    @property
    def name(self):
        return self._name_

    @property
    def value(self):
        return self._value_

    def __pyc_class_name__(self):
        return "Enum"

    def __repr__(self):
        return "<" + self.__pyc_class_name__() + "." + self._name_ + ": " + repr(self._value_) + ">"

    def __str__(self):
        return self.__pyc_class_name__() + "." + self._name_


    def __hash__(self):
        return hash(self._name_)

    def __copy__(self):
        return self

    def __deepcopy__(self, memo):
        return self

    @classmethod
    def _missing_(cls, value):
        return None

    @__pyc_metamethod__
    def __iter__(cls):
        return iter(cls.__pyc_members__)

    @__pyc_metamethod__
    def __reversed__(cls):
        return reversed(cls.__pyc_members__)

    @__pyc_metamethod__
    def __len__(cls):
        return len(cls.__pyc_members__)

    @__pyc_metamethod__
    def __contains__(cls, x):
        # 3.12: a member of the class, or a member's value.
        for m in cls.__pyc_members__:
            if m is x or m._value_ == x:
                return True
        return False

    @__pyc_metamethod__
    def __getitem__(cls, name):
        # Tested first: a dict read of a missing key does not raise
        # (issues/175).
        if name not in cls.__members__:
            raise KeyError(repr(name))
        return cls.__members__[name]

    @__pyc_metamethod__
    def __pyc_reversed__(cls):
        # reversed(x) dispatches __pyc_reversed__ (gen_class_pyda).
        return reversed(cls.__pyc_members__)

    @__pyc_metamethod__
    def __call__(cls, value):
        for m in cls.__pyc_members__:
            if m._value_ == value:
                return m
        r = cls._missing_(value)
        if r is None:
            raise ValueError(repr(value) + " is not a valid " + cls.__pyc_enum_name__)
        return r


class ReprEnum(Enum):
    pass


def unique(cls):
    # Consumed by the lowering: __pyc_enum_unique__ runs once the members
    # exist.
    return cls


# --- called by the lowered code ------------------------------------------

def __pyc_enum_add__(cls, m):
    for x in cls.__pyc_members__:
        if x._value_ == m._value_:
            cls.__members__[m._name_] = x
            return x
    cls.__pyc_members__.append(m)
    cls.__members__[m._name_] = m
    return m


def __pyc_enum_unique__(cls):
    dups = ""
    for name in cls.__members__:
        m = cls.__members__[name]
        if m._name_ != name:
            if dups:
                dups += ", "
            dups += name + " -> " + m._name_
    if dups:
        raise ValueError("duplicate values found in <enum '" + cls.__pyc_enum_name__ + "'>: " + dups)
    return cls


def __pyc_enum_next__(last_values):
    # Enum._generate_next_value_ (CPython 3.12) for a non-empty list: both of
    # its paths return the largest value plus one (the deprecated one only
    # warns). Its `start` is for an empty list, which the lowering handles.
    last_values.sort()
    return last_values[-1] + 1
