# A @property getter in a module reached only by IMPORT. The accessor
# injection runs before the builtin module is built and used to see only
# the files compiled directly, so this was refused ("this @property is
# not supported"). `area` is also a plain field of Plot: the choice
# between getter and field read is per receiver class, by dispatch.
import property_in_imported_module_helper as helper
from property_in_imported_module_helper import Box


class Plot:
    def __init__(self):
        self.area = 7


print(helper.Box(3, 4).area)
print(Box(2, 5).area)
print(Plot().area)
