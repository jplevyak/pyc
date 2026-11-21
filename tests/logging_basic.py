# issues/197: logging's common path -- basicConfig, module-level calls, the
# logger hierarchy, levels, a Formatter subclass, a name Filter, disable().
import logging
import sys

logging.basicConfig(stream=sys.stdout, level=logging.INFO)
logging.debug("hidden %d", 1)
logging.info("started with %d items, ratio %.2f", 3, 0.5)
logging.warning("plain warning")
logging.error(42)

log = logging.getLogger("app.db")
log.info("db up: %s", "yes")
log.setLevel(logging.ERROR)
log.warning("suppressed")
log.error("failed: %r", "x")
print(log.getEffectiveLevel(), logging.getLogger("app").getEffectiveLevel())
print(log.parent.name, logging.getLevelName(30), logging.getLevelName(7))

app = logging.getLogger("app")
print(log.parent.name)
h = logging.StreamHandler(sys.stdout)
h.setFormatter(logging.Formatter("[%(levelname)-8s] %(name)s: %(message)s"))
app.addHandler(h)
app.propagate = False
log.critical("both %s and %s", "a", "b")
child = app.getChild("net")
child.info("child info")
child.warning("child %d%%", 50)

class Upper(logging.Formatter):
    def format(self, record):
        return logging.Formatter.format(self, record).upper()

h2 = logging.StreamHandler(sys.stdout)
h2.setFormatter(Upper("%(name)s|%(levelno)d|%(message)s"))
other = logging.getLogger("other")
other.addHandler(h2)
other.propagate = False
other.warning("shout %s", "this")

class OnlyApp(logging.Filter):
    pass
r = logging.getLogger()
r.addFilter(logging.Filter("nope"))
logging.warning("filtered out")
print(r is logging.root, r.name, len(r.handlers))
logging.disable(logging.WARNING)
log.error("error still shown")
log.warning("disabled")
