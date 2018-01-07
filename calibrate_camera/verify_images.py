#!/usr/bin/python2

import numpy as np
import cv2
import sys
import os


objp = np.zeros((6*8, 3), np.float32)
objp[:,:2] = np.mgrid[0:8,0:6].T.reshape(-1, 2)
objpoints = []
imgpoints = []

images = sys.argv[1:]
if len(images) < 2:
    print("need at least 2 images")
    sys.exit(1)

criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

for i in images:
    if i == "backup.png":
        continue
    im = cv2.imread(i)
    gray = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)
    ret, corners = cv2.findChessboardCorners(gray, (8, 6), None)
    if ret == True:
        cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
    else:
        print(("Could not find chessboard in image " + i))
    cv2.drawChessboardCorners(im, (8, 6), corners, ret)
    cv2.imshow('corners', im)
    while True:
        print((i + ": k/space for KEEP, d/backspace for DELETE, q/escape to quit"))
        k = cv2.waitKey() & 255
        # print("key="+str(k))
        if (k == 8) or (k == 100):  # d
            if os.path.exists("backup.png"):
                os.unlink("backup.png")
            os.rename(i, "backup.png")
            print(("Image " + i + " renamed to backup.png"))
            break
        elif (k == 32) or (k == 107):   # k
            print(("Image " + i + " marked good"))
            imgpoints.append(corners)
            objpoints.append(objp)
            break
        elif (k == 27) or (k == 114):
            print("early quit")
            sys.exit(1)

cv2.destroyAllWindows()
