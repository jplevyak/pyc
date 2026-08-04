def _get_fd(item):
    if isinstance(item, int):
        return item
    return item.fileno()

def select(rlist, wlist, xlist, timeout=None):
    res_r = []
    res_w = []
    res_x = []
    if len(rlist) == 0:
        return (res_r, res_w, res_x)

    while True:
        for item in rlist:
            fd = _get_fd(item)
            if __pyc_c_call__(int, "_CG_net_poll_read", int, fd, int, 50) > 0:
                res_r.append(item)
        if len(res_r) > 0:
            break
        if timeout is not None:
            break

    return (res_r, res_w, res_x)
