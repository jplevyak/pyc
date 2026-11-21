# issues/206: argparse is refused at the import, with the reason: the
# attributes of its parse_args() result are named and typed at run time.
import argparse
p = argparse.ArgumentParser()
