import math

x = 47
y = 30
w = 640-2*x
h = 480-2*y
yoffset = -60 # change this if you want to crop higher up or lower down
            # more positive number means further down

def params():
    return (x, y, w, h, yoffset)

def deg(r):
    return r * 180.0 / math.pi

def rad(d):
    return d * math.pi / 180.0

s_width=540
s_left=(640-s_width)/2
s_height=220
s_topcrop=54
s_bottom=480-(480-s_height-s_topcrop)/2
s_centerangle=22.5
s_camheight=26.0
s_bottomfront=10.0
s_bottomangle=90.0-s_centerangle-deg(math.atan(s_bottomfront/s_camheight))

s_centertobottom=137.0
s_tanbottomangle=math.tan(rad(s_bottomangle))
s_planefront=s_centertobottom/s_tanbottomangle


def _sq_ypos(f):
    f_angle=deg(math.atan(f/s_camheight))
    f_relangle=90.0-f_angle-s_centerangle
    f_tana=math.tan(rad(f_relangle))
    return f_tana

def sq_ypos(f):
    f_tana=_sq_ypos(f)
    return s_planefront*f_tana+s_bottom-s_centertobottom

def sq_xpos(f,x):
    f_fdist=math.sqrt(s_camheight*s_camheight+f*f)
    f_side=s_planefront*x/f_fdist
    f_ret=f_side+s_left+s_width/2
    f_ok=(f_ret>=s_left and f_ret<=(s_left+s_width))
    return f_ret, f_ok

if __name__ == "__main__":
    y=sq_ypos(62.7)
    x,_=sq_xpos(62.7,0.0)
    print("62.7,0.0="+str(y)+","+str(x))
    y=sq_ypos(10.0)
    x,_=sq_xpos(10.0,-30)
    print("10.0,-30.0="+str(y)+","+str(x))
    y=sq_ypos(100.0)
    x,_=sq_xpos(100.0,30.0)
    print("100.0,30.0="+str(y)+","+str(x))
