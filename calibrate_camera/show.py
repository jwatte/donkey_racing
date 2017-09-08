#!/usr/bin/python2

import sys
import cv2
import cPickle as pickle
import numpy as np

if sys.argv[1].find(".pkl") != -1:
    src = np.array(pickle.load(open(sys.argv[1], "rb")))
else:
    src = cv2.imread(sys.argv[1])
cdata = pickle.load(open("calibrate.pkl", "rb"))
dst = cv2.remap(src, cdata['mapx'], cdata['mapy'], cv2.INTER_LINEAR)
x, y, w, h = (cdata['x'], cdata['y'], cdata['w'], cdata['h'])
x = 96
y = 63
w = 640-2*x # 448, div3 = 149
h = 480-2*y # 354, div2 = 177, div3 = 59
yoffset = -70 # change this if you want to crop higher up or lower down
            # larger number means further down
# only the bottom half of the image is interesting
crp = dst[y+(h/2)+yoffset:(y+h)+yoffset,x:(x+w)]
sml = cv2.resize(crp, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA)
cv2.rectangle(dst, (x, y), (x+w-1, y+h-1), (255, 0, 255))
cv2.imshow('resized', sml)
cv2.imshow('cropped', crp)
cv2.imshow('rectified', dst)
cv2.waitKey(0)
cv2.destroyAllWindows()
