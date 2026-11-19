# issues/197: %(lineno)d, %(funcName)s, %(pathname)s and the other fields
# CPython reads from the caller's stack frame are refused when the
# Formatter is made, before anything is printed: pyc has no frames to read
# them from, and a made-up value would be a silent wrong answer.
import logging
import sys

logging.basicConfig(stream=sys.stdout, format="%(lineno)d %(message)s")
logging.warning("x")
