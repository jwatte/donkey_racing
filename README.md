
Tools to help train the donkey car models.

`mktraining`: generates a synthetic set of images similar to what a rpi camera might capture.
horizontal FOV 60 degrees; resolution 320x240; random confetti on the floor for each image.
Libraries needed to build: GLFW and GLEW.

`blur`: blur a PNG image based on a mask given in another PNG image.

`teensy_32_carrier`: KiCad design files for the servo and receiver I/O board for the Teensy 3.2
microcontroller. Known flaws in revision 1.0:
  - Doesn't connect to Raspbery Pi "hat" port
  - Doesn't have enough I/Os to read BLDC motor sensors

All of these tools also want "stb" to be a checkout of github.com/nothings/stb as a peer 
to these directories.
