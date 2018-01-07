#!/usr/bin/python

import cv2
import pickle as pickle

class Rectify:
    def __init__(self, path=None):
        if not path:
            path = "calibrate.pkl"
        self.cdata = pickle.load(open(path, "rb"))
    def rectify(self, img):
        return cv2.remap(img, self.cdata['mapx'], self.cdata['mapy'], cv2.INTER_LINEAR)

if __name__ == "__main__":
    r = Rectify()
    src = cv2.imread("capture-gray.png")
    dst = r.rectify(src)
    cv2.imwrite("rectify-gray.png", dst)
    src = cv2.imread("capture-color.png")
    dst = r.rectify(src)
    cv2.imwrite("rectify-color.png", dst)
    cv2.imshow('color', dst)
    cv2.waitKey(0)
    cv2.destroyAllWindows()
