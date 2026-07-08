class Serial:
    def __init__(self, port=None, baudrate=9600):
        self.port = port
        self.baudrate = baudrate
        
    def read(self, size=1):
        return ""
        
    def write(self, data):
        return len(data)
        
    def close(self):
        pass
