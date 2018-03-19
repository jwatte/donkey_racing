import math

x = 94 # leaves 640-(2*x) pixels in center
y = 130 # leaves 480-(2*y) pixels in center
w = 640-2*x
h = 480-2*y
yoffset = 40 # change this if you want to crop higher up or lower down
             # more positive number means further down
scaledown = 1 # divide output rectified image by this much

def params():
    return (x, y, w, h, yoffset, scaledown)

# values and functions for attempted flat mapping

def deg(r):
    return r * 180.0 / math.pi

def rad(d):
    return d * math.pi / 180.0

# which area of picture to trust
s_widthpx=w
s_leftpx=(640-s_widthpx)/2
s_heightpx=h
s_toppx=(480-s_heightpx)/2
s_slope=0.0
s_slopescale=1.0

# measurements from camera placement
s_camheightcm=25.0
# s_centerangledeg=22.5 -- calculated as s_calcangledeg
s_frontatbottomcm=12.0
s_widthatbottomcm=54.0
#s_widthatbottomcm=58.0
#s_frontatcentercm=62.0
s_frontatcentercm=58.0  # calculated, this would be 62 cm at 22.5 deg; this is from measurement

s_calcangledeg = deg(math.atan2(s_frontatcentercm, s_camheightcm))
s_frontangledeg = deg(math.atan2(s_frontatbottomcm, s_camheightcm))
# print 's_calcangledeg = %f' % (90-s_calcangledeg,)
# print 's_frontangledeg = %f' % (90-s_frontangledeg,)
s_halffovydeg = s_calcangledeg - s_frontangledeg
print 's_halffovy = %f' % (s_halffovydeg,)
s_halfdist = 240 - s_toppx

# I now know:
# At 12 cm forward, I want to return 240 + s_halfdist
# At 58 cm forward, I want to return 240
# The angle down is s_calcangledeg - math.atan2(fcm, s_camheightcm)
# The position is 240 + math.sin(angledown) * s_halfdist / math.sin(halffov)
s_sinhalffov = math.sin(rad(s_halffovydeg))

# half-ass this number
s_hfrontmulpx = 170

# derived values

def shape_y(fcm):
    angledown = s_calcangledeg - deg(math.atan2(fcm, s_camheightcm))
    return 240 + math.sin(rad(angledown)) * s_halfdist / s_sinhalffov

def sq_ypos(fcm):
    # f_updeg=deg(math.atan(fcm/s_camheightcm))
    # f_down=(90.0-s_calcangledeg)-f_updeg
    # f_x=math.tan(rad(f_down))*s_slopescale
    # f_tandown = (-s_slope * f_x * f_x + f_x + s_slope)/s_slopescale
    # f_ret=s_vfrontmulpx*f_tandown+480/2.0
    f_ret = shape_y(fcm)
    f_ok=(f_ret>=s_toppx and f_ret<=s_toppx+s_heightpx)
    # print(('fcm=%.1f ret=%.1f' % (fcm, f_ret)))
    return f_ret, f_ok
    
def sq_xpos(fcm,dxcm):
    f_dist=math.sqrt(fcm*fcm+s_camheightcm*s_camheightcm)
    f_rtscale=dxcm/f_dist
    #f_rightdeg=math.atan(rtscale)
    #f_tanright=math.tan(f_rightdeg)
    f_ret=s_hfrontmulpx*f_rtscale+640/2.0
    f_ok=(f_ret>=s_leftpx and f_ret<=s_widthpx+s_leftpx)
    return f_ret, f_ok

if __name__ == "__main__":
    print(('hfrontmulpx=%.1f / %f (%.2f); vfrontmulpx=%.1f / %f (%.2f)' %
            (s_hfrontmulpx, s_widthpx/2.0, math.tan(rad(s_hfovhdeg)), s_vfrontmulpx, s_heightpx/2.0, math.tan(rad(s_hfovvdeg)))))
    print(('hfovh=%.1f / %f  hfovv=%.1f / %f' % (s_hfovhdeg, s_widthpx/2.0, s_hfovvdeg, s_heightpx/2.0)))
    y=sq_ypos(62.7)
    x,_=sq_xpos(62.7,0.0)
    print(("62.7,0.0="+str(y)+","+str(x)))
    y=sq_ypos(12.0)
    x,_=sq_xpos(12.0,-30)
    print(("10.0,-30.0="+str(y)+","+str(x)))
    y=sq_ypos(100.0)
    x,_=sq_xpos(100.0,30.0)
    print(("100.0,30.0="+str(y)+","+str(x)))
