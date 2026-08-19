# issues/113: a module that cannot be found is reported as an IMPORT
# error, naming both paths that were searched.
#
# It used to be recorded and deferred to IF1 building, by which time the
# undefined-name pass (issues/107) had already fired and reported the
# CONSEQUENCE -- minilight's `from ml import entry` failed with
# "name 'entry' is not defined" and never mentioned `ml` at all.
from nosuchpkg.sub import thing

print(thing(1))
