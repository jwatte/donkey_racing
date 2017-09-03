
  1) Capture a number of images of a 9x7 checkerboard held facing straight
  against the camera in different locations in the field of view.

    python capture_images.py

  2) Verify that the images have nice and straight gridlines. If the detected
  grid jumps around in the image. remove it. Press space to mark an image 
  good, anything else to mark it bad. You must manually remove the images 
  later; the marking is just to create a textual log.

    python verify_images.py *.png

  3) Calculate calibration coefficients given the captured images that are 
  good.

    python calibrate_camera.py *.png
  
  This generates the calibrate.pkl file. (It will overwrite any previous such 
  file so make backups if you don't want this!)

  4) Verify that a "good chunk" of the image is usable -- you want the edges 
  to bend in with black, and perhaps even some mirroring, but if there are 
  black ovals in the rectified image, you got it wrong (hold the checkerboard 
  more flat-facing the camera while capturing images.)
  The smaller image is what will go into the neural network.

    python show.py some-image.png

  This will load the calibrate.pkl file, load an image, rectify it, and crop 
  plus resize to make it the way the neural network wants it.

  5) Finally, generate a C file that contains the re-mapping table.

    python maketable.py

  Note the "x" and "y" and "yoffset" variables from show.py should match the 
  ones in "maketable.py" -- if you change nothing, that's already true.
  This generates "table.cpp" and "table.h" which can be included in the pilot
  program.

