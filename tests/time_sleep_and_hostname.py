# issues/041: time.sleep really sleeps (it was a no-op) and raises ValueError
# for a negative length; socket.gethostname is the real host name (it was
# the constant "localhost"; checked for consistency, not printed, since
# the expected output must hold on any machine).
import time
import socket
t0 = time.time()
time.sleep(0.25)
dt = time.time() - t0
print(dt >= 0.25, dt < 2.0)
print(time.sleep(0) is None)
try:
    time.sleep(-1)
except ValueError as e:
    print("ValueError:", e)
h = socket.gethostname()
print(len(h) > 0, h == socket.gethostname())
