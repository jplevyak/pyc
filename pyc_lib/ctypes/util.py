# ctypes.util: find_library("name") is read at compile time as `-lname`
# (python_ifa_ctypes.cc); the name exists so the import binds.
find_library = 0
