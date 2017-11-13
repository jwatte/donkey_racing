import math

#x = 104 # leaves 444 pixels in center
x = 72  # leaves 496 pixels in center
y = 94  # 12 cm ahead of camera center
        # at bottom, 444 pixels in center crosses 6*9 = 54 centimeters
w = 640-2*x
h = 480-2*y
yoffset = -72 # change this if you want to crop higher up or lower down
            # more positive number means further down

def params():
    return (x, y, w, h, yoffset)

def deg(r):
    return r * 180.0 / math.pi

def rad(d):
    return d * math.pi / 180.0

# which area of picture to trust
s_widthpx=w
s_leftpx=(640-s_widthpx)/2
s_heightpx=h
s_toppx=(480-s_heightpx)/2

# measurements from camera placement
s_camheightcm=25.0
# s_centerangledeg=22.5 -- calculated as s_calcangledeg
s_frontatbottomcm=12.0
#s_widthatbottomcm=54.0
s_widthatbottomcm=70.0
#s_frontatcentercm=76.0  # calculated, this would be 62 cm at 22.5 deg; this is from measurement
s_frontatcentercm=62.0

# derived values
s_disttobottomcm=math.sqrt(s_camheightcm*s_camheightcm+s_frontatbottomcm*s_frontatbottomcm)
s_hfovhdeg=deg(math.atan(s_widthatbottomcm*0.5/s_disttobottomcm))  # full fov is about 90 !?
s_calcangledeg=90.0-deg(math.atan(s_frontatcentercm/s_camheightcm)) # approximately 20 deg
s_bottomangledeg=deg(math.atan(s_frontatbottomcm/s_camheightcm))    # full fov is about 90
s_hfovvdeg=90.0-s_calcangledeg-s_bottomangledeg
s_vfrontmulpx=s_heightpx/2.0/math.tan(rad(s_hfovvdeg))
s_hfrontmulpx=s_widthpx/2.0/math.tan(rad(s_hfovhdeg))

def sq_ypos(fcm):
    f_updeg=deg(math.atan(fcm/s_camheightcm))
    f_down=90.0-s_calcangledeg-f_updeg
    f_tandown=math.tan(rad(f_down))
    f_ret=s_vfrontmulpx*f_tandown+480/2.0
    f_ok=(f_ret>=s_toppx and f_ret<=s_toppx+s_heightpx)
    return f_ret, f_ok
    
def sq_xpos(fcm,dxcm):
    f_dist=math.sqrt(fcm*fcm+s_camheightcm*s_camheightcm)
    f_tanright=math.tan(dxcm/f_dist)
    f_ret=s_hfrontmulpx*f_tanright+640/2.0
    f_ok=(f_ret>=s_leftpx and f_ret<=s_widthpx+s_leftpx)
    return f_ret, f_ok

if __name__ == "__main__":
    print('hfrontmulpx=%.1f / %f (%.2f); vfrontmulpx=%.1f / %f (%.2f)' %
            (s_hfrontmulpx, s_widthpx/2.0, math.tan(rad(s_hfovhdeg)), s_vfrontmulpx, s_heightpx/2.0, math.tan(rad(s_hfovvdeg))))
    print('hfovh=%.1f / %f  hfovv=%.1f / %f' % (s_hfovhdeg, s_widthpx/2.0, s_hfovvdeg, s_heightpx/2.0))
    y=sq_ypos(62.7)
    x,_=sq_xpos(62.7,0.0)
    print("62.7,0.0="+str(y)+","+str(x))
    y=sq_ypos(12.0)
    x,_=sq_xpos(12.0,-30)
    print("10.0,-30.0="+str(y)+","+str(x))
    y=sq_ypos(100.0)
    x,_=sq_xpos(100.0,30.0)
    print("100.0,30.0="+str(y)+","+str(x))
