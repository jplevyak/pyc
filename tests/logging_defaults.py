# issues/197: the defaults, all on stderr: a logger with no handlers anywhere uses
# lastResort (WARNING, message only); the first module-level call
# configures the root logger with BASIC_FORMAT.
import logging

q = logging.getLogger("q")
q.info("hidden: below lastResort's level")
q.warning("no handlers: %s", "lastResort")
logging.info("hidden: root is at WARNING")
logging.error("configured %d", 1)
q.warning("now through root")
logging.getLogger("q.r").critical("deep %s", [1, 2])
