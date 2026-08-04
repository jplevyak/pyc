AF_INET = 2
SOCK_STREAM = 1
SOL_SOCKET = 65535
SO_REUSEADDR = 4

class socket:
    def __init__(self, family=AF_INET, type=SOCK_STREAM, proto=0, fd=-1):
        self.family = family
        self.type = type
        self.proto = proto
        if fd >= 0:
            self.fd = fd
        else:
            self.fd = __pyc_c_call__(int, "_CG_net_socket", int, family, int, type, int, proto)

    def fileno(self):
        return self.fd

    def __str__(self):
        return "<socket>"

    def __repr__(self):
        return "<socket>"

    def bind(self, address):
        host = address[0]
        port = address[1]
        return __pyc_c_call__(int, "_CG_net_bind", int, self.fd, str, host, int, port)

    def listen(self, backlog=5):
        return __pyc_c_call__(int, "_CG_net_listen", int, self.fd, int, backlog)

    def accept(self):
        cfd = __pyc_c_call__(int, "_CG_net_accept", int, self.fd)
        sock = socket(self.family, self.type, self.proto, cfd)
        return (sock, ("127.0.0.1", 0))

    def recv(self, bufsize, flags=0):
        return __pyc_c_call__(str, "_CG_net_read_str", int, self.fd, int, bufsize)

    def send(self, data, flags=0):
        return __pyc_c_call__(int, "_CG_net_write_str", int, self.fd, str, data)

    def close(self):
        if self.fd >= 0:
            __pyc_c_call__(int, "_CG_net_close", int, self.fd)
            self.fd = -1

    def setsockopt(self, level, optname, value):
        pass

def gethostname():
    return "localhost"
