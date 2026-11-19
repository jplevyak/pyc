# pyc shim for the standard `typing` module (issues/195).
#
# pyc ignores annotations: `def f(d: Dict[str, int]) -> Optional[int]`
# never evaluates them. So most of this module only has to bind the names
# a program imports. What CPython DOES evaluate at run time is a type
# expression outside an annotation, an alias like `Pathlike = Union[Path,
# str]`: every special form here is a value whose subscript is itself, so
# such an alias compiles and is never otherwise usable, which is all a
# program does with one. (`X: TypeAlias = ...` is dropped by the frontend
# before it gets here; see python_ifa_dataclass.cc.)
#
# Not here, so that a program needing them is refused at its import rather
# than mis-typed: NamedTuple, TypedDict, Generic and Protocol as bases.

TYPE_CHECKING = False


class _SpecialForm:
    def __getitem__(self, item):
        return self


Any = _SpecialForm()
Optional = _SpecialForm()
Union = _SpecialForm()
Callable = _SpecialForm()
ClassVar = _SpecialForm()
Final = _SpecialForm()
Literal = _SpecialForm()
TypeAlias = _SpecialForm()
Type = _SpecialForm()
Dict = _SpecialForm()
DefaultDict = _SpecialForm()
List = _SpecialForm()
Set = _SpecialForm()
FrozenSet = _SpecialForm()
Tuple = _SpecialForm()
Sequence = _SpecialForm()
MutableSequence = _SpecialForm()
Mapping = _SpecialForm()
MutableMapping = _SpecialForm()
Iterable = _SpecialForm()
Iterator = _SpecialForm()
Generator = _SpecialForm()
IO = _SpecialForm()
TextIO = _SpecialForm()
BinaryIO = _SpecialForm()
NoReturn = _SpecialForm()


def TypeVar(name):
    return _SpecialForm()


def cast(typ, val):
    return val


def overload(f):
    return f


def final(f):
    return f
