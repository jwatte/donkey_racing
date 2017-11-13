#!/usr/bin/python2

import sys
import cv2
import cPickle as pickle
import numpy as np
import localcrop
import rectify

if sys.argv[1].find(".pkl") != -1:
    src = np.array(pickle.load(open(sys.argv[1], "rb")))
else:
    src = cv2.imread(sys.argv[1])
rec = rectify.Rectify()
dst = rec.rectify(src)
# override for ROI
(x, y, w, h, yoffset) = localcrop.params()
# only the bottom half of the image is interesting
crp = dst[y+(h/2)+yoffset:(y+h)+yoffset,x:(x+w)]
sml = cv2.resize(crp, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA)
cv2.rectangle(dst, (x, y), (x+w-1, y+h-1), (255, 0, 255))
cv2.imshow('resized', sml)
cv2.imshow('cropped', crp)
cv2.imshow('rectified', dst)
cv2.waitKey(0)
cv2.destroyAllWindows()
