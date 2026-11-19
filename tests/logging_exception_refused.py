# issues/197: Logger.exception() needs the active exception's traceback,
# which pyc does not have, so the shim does not define it and the call is
# refused at compile time rather than logging without the traceback.
import logging

lg = logging.getLogger("x")
lg.exception("failed")
