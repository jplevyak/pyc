# time: the clocks, struct_time, gmtime/localtime, mktime, strftime of a time tuple, asctime/ctime (issues/207).
import time
g = time.gmtime(1e9)
print(g, g.tm_year, g[0], len(g), tuple(g), list(time.gmtime(0)), time.gmtime(1.9) == time.gmtime(1), time.gmtime(-1.5))
print(time.strftime("%Y-%m-%d %H:%M:%S %a %b %j %A %B %p %y %%", g))
print(time.strftime("%Y %m %d", (2020, 2, 29, 12, 30, 0, 5, 60, 0)), time.strftime("%d/%m", (2020, 0, 0, 0, 0, 0, 0, 0, 0)))
print(time.asctime(g), time.asctime((2024, 12, 25, 7, 5, 3, 2, 360, 0)))
lt = time.localtime(1e9)
print(time.mktime(lt), time.mktime(time.localtime(0)), time.localtime(time.mktime(lt)) == lt)
print(time.ctime(1e9) == time.asctime(time.localtime(1e9)), time.strftime("%H:%M", lt) == time.strftime("%H:%M", time.localtime(1e9)))
a = time.perf_counter(); b = time.perf_counter()
print(b >= a, time.monotonic() > 0, time.time_ns() // 10**9 - int(time.time()) in (0, 1), time.process_time() >= 0, isinstance(time.perf_counter_ns(), int))
for bad in [(2020, 13, 1, 0, 0, 0, 0, 1, 0), (2020, 1, 32, 0, 0, 0, 0, 1, 0), (2020, 1, 1, 24, 0, 0, 0, 1, 0), (2020, 1, 1, 0, 60, 0, 0, 1, 0), (2020, 1, 1, 0, 0, 62, 0, 1, 0), (2020, 1, 1, 0, 0, 0, -1, 1, 0), (2020, 1, 1, 0, 0, 0, -2, 1, 0), (2020, 1, 1, 0, 0, 0, -8, 1, 0), (2020, 1, 1, 0, 0, 0, 9, 1, 0), (2020, 1, 1, 0, 0, 0, 0, 367, 0)]:
    try:
        print(time.strftime("%Y", bad))
    except ValueError as e:
        print("ValueError", e)
