# Helper for module_attribute_store.py: a global assigned only under
# `global` (NODES, as othello2 has) and one bound at module level.
COUNT = 0

def bump():
    global COUNT
    COUNT = COUNT + 1

def reset_nodes():
    global NODES
    NODES = 0

def visit():
    global NODES
    NODES = NODES + 1
