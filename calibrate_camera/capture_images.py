#!/usr/bin/python2
import cv2
import time

cap = cv2.VideoCapture(0)
cap.set(cv2.cv.CV_CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.cv.CV_CAP_PROP_FRAME_HEIGHT, 480)
s, im0 = cap.read()
s, im0 = cap.read()
s, im0 = cap.read()
s, im0 = cap.read()
s, im0 = cap.read()
images=[]
cv2.waitKey(1)
cv2.waitKey(1)
cv2.waitKey(5000)
print("press a key for each picture (or wait 3 seconds)")
for i in range(0, 30):
    cv2.waitKey(3000)
    print("capture")
    s, im0 = cap.read()
    s, im0 = cap.read()
    s, im0 = cap.read()
    s, im0 = cap.read()
    s, im0 = cap.read()
    # cv2.cvtColor(im0, cv2.cv.CV_RGB2BGR, im0)
    gray = cv2.cvtColor(im0, cv2.COLOR_BGR2GRAY)
    ret, corners = cv2.findChessboardCorners(gray, (8, 6), None)
    if ret:
        images.append(im0)
    else:
        print("oops, no checkerboard detected")
    if len(images) >= 16:
        break
cap.release()

n = 1
for im in images:
    name = 'cal-image-%03d.png' % (n,)
    print("saving " + name)
    cv2.imwrite(name, im)
    n += 1 

print("Done")
