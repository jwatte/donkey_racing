
import pickle as pickle

image = []
for y in range(0, 480):
    row = []
    yy = y / 480.0
    for x in range(0, 640):
        row.append((x / 640.0, yy, 0))
    image.append(row)

pickle.dump(image, open("flt-640x480.pkl", "wb"))
