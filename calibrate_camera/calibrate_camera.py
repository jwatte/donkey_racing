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

print("loading images")
for i in images:
    print(i)
    im = cv2.imread(i)
    gray = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)
    ret, corners = cv2.findChessboardCorners(gray, (8, 6), None)
    if ret == True:
        cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
        slp = 10000
    else:
        print("Could not find chessboard in image " + i)
        print("Make sure that you verify images and sift out bad images first")
        sys.exit(1)
    imgpoints.append(corners)
    objpoints.append(objp)

print("Loaded %d images; calibrating" % (len(images),))

ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, gray.shape[::-1], None, None)
h, w = im.shape[:2]
newcameramtx, roi = cv2.getOptimalNewCameraMatrix(mtx, dist, (w, h), 1, (w, h))
print("roi=" + repr(roi) + "; newcameramtx=" + repr(newcameramtx))
if (roi[2] == 0) or (roi[3] == 0):
    roi = (0, 0, w, h)

mapx, mapy = cv2.initUndistortRectifyMap(mtx, dist, None, newcameramtx, (w, h), 5)
x, y, w, h = roi

print("Saving parameters to calibrate.pkl")
import cPickle as pickle
pickle.dump({'mapx':mapx, 'mapy':mapy, 'w':w, 'h':h, 'x':x, 'y': y}, open("calibrate.pkl", "wb"))
print("Done calibrating; here's an undistorted image")

dst = cv2.remap(im, mapx, mapy, cv2.INTER_LINEAR)
dst = dst[y:h+h, x:x+w]
cv2.imshow('cropped, rectified, %dx%d' % (w, h), dst)
cv2.waitKey(30000)
cv2.destroyAllWindows()


