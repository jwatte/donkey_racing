#!/usr/bin/python2

import cv2

class Capture:
    def __init__(self):
        self.cap = cv2.VideoCapture(0)
        self.cap.set(cv2.cv.CV_CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv2.cv.CV_CAP_PROP_FRAME_HEIGHT, 480)
    def capture(self):
        s, im0 = self.cap.read()
        s, im0 = self.cap.read()
        s, im0 = self.cap.read()
        s, im0 = self.cap.read()
        s, im0 = self.cap.read()
        gray = cv2.cvtColor(im0, cv2.COLOR_BGR2GRAY)
        return gray, im0
    def close(self):
        self.cap.relase()
        self.cap = None

if __name__ == "__main__":
    c = Capture()
    gray, color = c.capture()
    cv2.imwrite("capture-gray.png", gray)
    cv2.imwrite("capture-color.png", color)
    cv2.imshow('color', color)
    cv2.waitKey(0)
    cv2.destroyAllWindows()
