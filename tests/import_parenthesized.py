# issues/192: `from m import (...)` across lines, with comments and a
# trailing comma. Inside the parentheses lines join implicitly, as inside
# any other bracket.
from from_import_helper import (
    add,          # a function
    triple as tri,
    # a whole-line comment
    Point,
)
from from_import_helper import (GREETING)

print(add(1, 2), tri(3), Point(4).doubled(), GREETING)
