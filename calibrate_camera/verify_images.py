#!/usr/bin/python2

import numpy as np
import cv2
import sys


objp = np.zeros((6*8, 3), np.float32)
objp[:,:2] = np.mgrid[0:8,0:6].T.reshape(-1, 2)
objpoints = []
imgpoints = []

images = sys.argv[1:]
if len(images) < 2:
    print "need at least 2 images"
    sys.exit(1)

criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

for i in images:
    im = cv2.imread(i)
    gray = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)
    ret, corners = cv2.findChessboardCorners(gray, (8, 6), None)
    if ret == True:
        cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
        slp = 10000
    else:
        print("Could not find chessboard in image " + i)
        slp = 500
    cv2.drawChessboardCorners(im, (8, 6), corners, ret)
    cv2.imshow('corners', im)
    k = cv2.waitKey(slp)
    if k != 32:
        print("Image " + i + " marked bad")
    elif ret:
        print("Image " + i + " marked good")
        imgpoints.append(corners)
        objpoints.append(objp)

cv2.destroyAllWindows()
