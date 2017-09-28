
x = 50
y = 63
w = 640-2*x # 448, div3 = 149
h = 480-2*y # 354, div2 = 177, div3 = 59
yoffset = -60 # change this if you want to crop higher up or lower down
            # more positive number means further down

def params():
    return (x, y, w, h, yoffset)

