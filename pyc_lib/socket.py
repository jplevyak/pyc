AF_INET = 2
SOCK_STREAM = 1
SOL_SOCKET = 65535
SO_REUSEADDR = 4

class socket:
    def __init__(self, family=AF_INET, type=SOCK_STREAM, proto=0):
        self.family = family
        self.type = type
        self.proto = proto
        
    def bind(self, address):
        pass
        
    def listen(self, backlog):
        pass
        
    def accept(self):
        return (self, ("0.0.0.0", 0))
        
    def recv(self, bufsize, flags=0):
        return ""
        
    def send(self, data, flags=0):
        return len(data)
        
    def close(self):
        pass
        
    def setsockopt(self, level, optname, value):
        pass

def gethostname():
    return "localhost"
