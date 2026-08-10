# issues/041: colorsys was a no-op stub (every function unconditionally
# returned (0.0, 0.0, 0.0)) -- now a real port of CPython's colorsys.py.
import colorsys

print(colorsys.hsv_to_rgb(0.5, 1.0, 1.0))
print(colorsys.rgb_to_hsv(0.0, 1.0, 1.0))
print(colorsys.rgb_to_hsv(0.5, 0.5, 0.5))
print(colorsys.hls_to_rgb(0.3, 0.5, 0.8))
print(colorsys.rgb_to_hls(0.2, 0.6, 0.9))
print(colorsys.rgb_to_hls(0.5, 0.5, 0.5))
print(colorsys.rgb_to_yiq(0.5, 0.5, 0.5))
print(colorsys.yiq_to_rgb(0.5, 0.1, -0.1))
print(colorsys.hsv_to_rgb(0.0, 0.0, 0.5))
