# issues/197: level names, a Handler subclass, a Filter subclass, a StringIO
# stream, per-handler levels.
import logging
import io
import sys

# Level names, and a level given by name.
print(logging.getLevelName(logging.DEBUG), logging.getLevelName(35))
logging.addLevelName(35, "NOTICE")
print(logging.getLevelName(35))

# A handler collecting records, a Filter subclass, a StringIO stream.
class Collect(logging.Handler):
    def __init__(self):
        logging.Handler.__init__(self)
        self.records = []

    def emit(self, record):
        self.records.append(self.format(record))


class NoSecrets(logging.Filter):
    def filter(self, record):
        return record.getMessage().find("secret") < 0


lg = logging.getLogger("svc")
lg.setLevel("DEBUG")
c = Collect()
c.setFormatter(logging.Formatter("%(levelno)02d %(name)s %(message)s"))
c.addFilter(NoSecrets())
lg.addHandler(c)
buf = io.StringIO()
sh = logging.StreamHandler(buf)
sh.setLevel(logging.WARNING)
sh.setFormatter(logging.Formatter("%(levelname)s|%(message)r"))
lg.addHandler(sh)
lg.propagate = False
lg.debug("d %s", "one")
lg.info("the secret is %d", 42)
lg.log(35, "notice %.1f", 1.25)
lg.warning("w %s %s", "a", 2)
lg.critical("c")
for r in c.records:
    print(r)
print(buf.getvalue(), end="")
print(lg.hasHandlers(), lg.isEnabledFor(logging.DEBUG), lg.getEffectiveLevel())
lg.removeHandler(sh)
print(len(lg.handlers))
