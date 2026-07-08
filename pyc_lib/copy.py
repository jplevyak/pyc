# pyc shim for the standard `copy` module

def copy(obj):
    return __pyc_primitive__(__pyc_symbol__("copy"), obj)

def deepcopy(obj):
    return copy(obj)
