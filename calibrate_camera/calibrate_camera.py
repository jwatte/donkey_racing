#!/usr/bin/python2

import numpy as np
import cv2
import sys


fisheye = False
if fisheye:
    objp = np.zeros((1, 6*8, 3), np.float64)
    objp[0,:,:2] = np.mgrid[0:8,0:6].T.reshape(-1, 2)
else:
    objp = np.zeros((6*8, 3), np.float32)
    objp[:,:2] = np.mgrid[0:8,0:6].T.reshape(-1, 2)
objpoints = []
imgpoints = []

images = sys.argv[1:]
if len(images) < 2:
    print("need at least 2 images")
    print("argv is " + repr(sys.argv))
    sys.exit(1)

criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

print("loading images")
for i in images:
    if (i == 'backup.png'):
        continue
    print(i)
    im = cv2.imread(i)
    gray = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)
    ret, corners = cv2.findChessboardCorners(gray, (8, 6), None)
    if ret == True:
        cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
    else:
        print(("Could not find chessboard in image " + i))
        print("Make sure that you verify images and sift out bad images first")
        sys.exit(1)
    imgpoints.append(corners)
    objpoints.append(objp)

print(("Loaded %d images; calibrating" % (len(images),)))

Kout = np.zeros((3, 3))
Dout = np.zeros((4,))
h, w = im.shape[:2]
if fisheye:
    #imgpoints = [x.reshape(-1, 2) for x in imgpoints]
    #print(repr(imgpoints))
    num = len(imgpoints)
    chessboard_model = objpoints
    rvecs = [np.zeros((1, 1, 3), dtype=np.float64) for i in range(num)]
    tvecs = [np.zeros((1, 1, 3), dtype=np.float64) for i in range(num)]
    ret, Kmtx, Ddist, rvecs, tvecs = cv2.omnidir.calibrate(
            chessboard_model, imgpoints, (w, h),
                Kout, Dout, flags=cv2.fisheye.CALIB_RECOMPUTE_EXTRINSIC+cv2.fisheye.CALIB_FIX_SKEW)
    Pnewcameramtx = cv2.omnidir.estimateNewCameraMatrixForUndistortRectify(
            Kmtx, Ddist, (h, w), None)
    roi = (0, 0, w, h)
    #print(Kout)
    #print(Ddist)
    print(("Kmtx=" + repr(Kmtx) + "\nDdist=" + repr(Ddist)))
    print(("roi=" + repr(roi) + "\nnewcameramtx=" + repr(Pnewcameramtx)))
    mapx, mapy = cv2.fisheye.initUndistortRectifyMap(Kmtx, Ddist,
            np.array([[1., 0., 0.], [0., 1., 0.], [0., 0., 1.]]),
            Pnewcameramtx, (w, h), cv2.CV_32FC1)
else:
    ret, Kmtx, Ddist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, gray.shape[::-1],
                Kout, Dout, flags=0)
    Pnewcameramtx, roi = cv2.getOptimalNewCameraMatrix(Kmtx, Ddist, (h, w), 1, (h, w))
    print(("roi=" + repr(roi) + "\nnewcameramtx=" + repr(Pnewcameramtx)))
    if (roi[2] == 0) or (roi[3] == 0):
        roi = (0, 0, w, h)
    mapx, mapy = cv2.initUndistortRectifyMap(Kmtx, Ddist, None, Pnewcameramtx, (w, h), 5)

x, y, w, h = roi

print("Saving parameters to calibrate.pkl")
import pickle as pickle
pickle.dump({'mapx':mapx, 'mapy':mapy, 'w':w, 'h':h, 'x':x, 'y': y}, open("calibrate.pkl", "wb"))
print("Done calibrating; here's an undistorted image")

dst = cv2.remap(im, mapx, mapy, cv2.INTER_LINEAR)
x, y, w, h = roi
print(("roi="+repr(roi)))
#dst = dst[y:h+h, x:x+w] # crop
cv2.rectangle(dst, (x,y), (x+w-1,y+h-1), (255,0,255), 1)
cv2.imshow('rectified', dst)
cv2.waitKey(30000)
cv2.destroyAllWindows()


