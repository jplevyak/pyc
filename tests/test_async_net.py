def create_socket() -> int:
    return __pyc_c_call__(int, "socket", int, 2, int, 1, int, 6)

def net_connect(fd: int, host: str, port: int) -> int:
    return __pyc_c_call__(int, "_CG_net_connect", int, fd, str, host, int, port)

def net_write(fd: int, s: str) -> int:
    return __pyc_c_call__(int, "_CG_net_write_str", int, fd, str, s)

def net_read(fd: int, max_len: int) -> str:
    return __pyc_c_call__(str, "_CG_net_read_str", int, fd, int, max_len)

async def wait_write(fd: int):
    __pyc_c_call__(int, "__pyc_net_wait_write__", int, fd)

async def wait_read(fd: int):
    __pyc_c_call__(int, "__pyc_net_wait_read__", int, fd)

async def do_request():
    fd = create_socket()
    print("Socket FD:", fd)
    res = net_connect(fd, "example.com", 80)
    print("Connect result:", res)
    
    __pyc_c_call__(int, "__pyc_net_wait_write__", int, fd)

    print("Socket is writable! Connected.")
    
    req = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n"
    written = net_write(fd, req)
    print("Bytes written:", written)
    
    print("Waiting for response...")
    __pyc_c_call__(int, "__pyc_net_wait_read__", int, fd)
    
    data = net_read(fd, 256)
    print("Received data!")
    print(data)

async def main():
    print("Starting network test...")
    await do_request()
    print("Done")

main()
res = __pyc_c_call__(int, "_CG_run_coro", int, main())
