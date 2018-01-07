#!/usr/bin/python2
import cv2
import time
import capture

NUM_NEEDED=20
NUM_CAPTURE = 2*NUM_NEEDED
images=[]
cv2.waitKey(1)
cv2.waitKey(1)
cv2.waitKey(5000)
print("press a key for each picture (or wait 3 seconds)")
with capture.Capture() as c:
    for i in range(0, NUM_CAPTURE):
        cv2.waitKey(3000)
        print("capture")
        gray, color = c.capture()
        ret, corners = cv2.findChessboardCorners(gray, (8, 6), None)
        if ret:
            images.append(im0)
        else:
            print("oops, no checkerboard detected")
        if len(images) >= NUM_NEEDED:
            break

n = 1
for im in images:
    name = 'cal-image-%03d.png' % (n,)
    print(("saving " + name))
    cv2.imwrite(name, im)
    n += 1 

print("Done")
