import math

x = 47
y = 30
w = 640-2*x
h = 480-2*y
yoffset = -60 # change this if you want to crop higher up or lower down
            # more positive number means further down

def params():
    return (x, y, w, h, yoffset)

s_down=20
s_near=150
s_hfovy=45
s_cy=240
s_hwidth=270
s_hfovx=60
s_cx=320

def squareParams():
    return (s_down, s_near, s_hfovy, s_cy, s_hwidth, s_fovx, s_cx)

def squareMap(dx, dy):
    
