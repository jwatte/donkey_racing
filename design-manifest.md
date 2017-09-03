
2017-09-02:

This thread on donkeycar.slack.com channel #help captures my thinking around 
how Teensy vs CPU should be balanced, and what the communication between the 
two should look like:


jwatte [2 hours ago] 
I'm not a fan of "generic" low-level access, such as "StandardFirmata," because performance is almost always terrible.


jwatte [2 hours ago] 
Consider the PinPulseIn class I'm using to read RC PWM. That's not a standard Firmata feature. It can't be written from the host using Firmata -- it needs interrupt handlers on the MCU with specific timing constraints.


jwatte [2 hours ago] 
Separately, I want dead man switch functionality -- if the Pi hasn't sent a command in 300 milliseconds or whatever, the MCU should tell the ESC to shut down.

jwatte [2 hours ago] 
That's logic on the MCU side, which again StandardFirmata doesn't do.


jwatte [2 hours ago] 
(Same thing for using the third channel as an Enable, and so on)


jwatte [2 hours ago] 
Which leaves the Firmata wire protocol for possible re-use.


jwatte [2 hours ago] 
But that's very low level -- "analog pin 3 has value 87" -- and I prefer higher-level semantics in communication.


jwatte [2 hours ago] 
Looking at AlexMos or Pixhawk or similar devices, they don't use Firmata from what I can tell for presumably similar reasons. (I'm not an expert on those though) (edited)


chr1sa [28 minutes ago] 
Yes, you make a good point, although obviously any specify Firmata functions can be replaced with a more efficient one with the same API. I actually helped write the original Pixhawk implementation and we wrote out own Hardware Abstraction Layer to do the same thing as Firmata. It's proven essential as the range of hardware we support expands.


chr1sa [28 minutes ago] 
The point is the standard API rather than the specific implementation


jwatte [27 minutes ago] 
I understand. I'm totally open to a standard API that specifies the semantic levels I'm interested in.


jwatte [27 minutes ago] 
As in "drive forward" rather than "set servo on pin 3 to duty cycle 1760"


jwatte [26 minutes ago] 
And "main battery voltage is 11.3V" rather than "analog pin 18 has a value of 932"


jwatte [24 minutes ago] 
I'd be OK to think of it as ROS topics over a serial wire


chr1sa [23 minutes ago] 
That leads to a debate about what level of abstraction should live at the Pi/OpenMV vs the microprocessor that handles I/O. In your example, I'd actually lean towards keeping "drive forward" at the main code level and keep the Teensy focused on lower level stuff like "send PMW to Channel". That's the way we do it with Pixhawk's HAL

chr1sa [20 minutes ago] 
The idea is to not have to reprogram the Teensy at all. It really should be a black box, which means that you need flexibility on what connects to what. In your temperature examples, you'd need a setup API that tells the Teensy what pins are connected to the battery, the calibration of the voltage divider, the capacity of the battery and how many cells it has.


jwatte [19 minutes ago] 
I suppose we have a difference of opinion then!

jwatte [18 minutes ago] 
I've tried that in the past, and found it to perform less well than higher-level PDUs.

jwatte [18 minutes ago] 
But I'm open to data. How, for example, would you implement the following feature?

jwatte [18 minutes ago] 
- when car starts up, it's in stand-by mode

jwatte [17 minutes ago] 
- flipping channel 3 to "on" (above 1500 PWM in) switches it to "manual drive" mode where throttle/steer just drives it

jwatte [17 minutes ago] 
- booting the controlling CPU makes the CPU receive the drive inputs and an echo of remote PWM/control channel inputs

jwatte [17 minutes ago] 
- CPU can decide to take the Teensy out of manual mode, and instead go to automatic mode

jwatte [17 minutes ago] 
- in automatic mode, the Teensy drives forward with a function that's max(throttle-input, automatic-input) (this is dead man's grip)

jwatte [16 minutes ago] 
- if the CPU stops sending data for 300 milliseconds, the Teensy goes back to manual mode


chr1sa [16 minutes ago] 
There definitely is a performance hit with abstraction layers, but remember that with the Teensy we've got way more power than we need. The days of having to write effecient non-blocking code on 8 bit processors are going away. We've got performance to spare


jwatte [16 minutes ago] 
(CPU in this case is Pi or OpenMV or Jetson or whatever)


jwatte [16 minutes ago] 
The performance I worry about is communications latency, not bits of RAM :slightly_smiling_face:


jwatte [15 minutes ago] 
So, how would the system behavior I outline above (which I've found to be highly productive for my own development) be implemented in the "treat the Teensy as an I/O co-processor" model?


chr1sa [12 minutes ago] 
That's fair, but do look at what we did with Pixhawk. We needed way better performance than what we need with cars (10ms) and had no problem with abstraction. All the code bases that chose not to use a HAL ended up being dead ends or just for twitch-optimized racing quads that don't have any autonomy


jwatte [10 minutes ago] 
I understand your perspective. As I said, I agree that a key/value protocol is valuable. I disagree on the level of smarts distribution between MCU and CPU.

jwatte [9 minutes ago] 
I e, "can drive manually while the CPU is booting" is highly valuable to me.

jwatte [9 minutes ago] 
"dead man's grip" safety with a pretty aggressive timeout based on CPU->MCU commands is important to me (because CPUs crash more than MCUs)


jwatte [8 minutes ago] 
"power off when the battery has too low voltage" is very important to me, having lost LiPos to forgetfulness before. (edited)


jwatte [7 minutes ago] 
"GUI based interface for switching training modes and such" is important to me, and has currently been implemented with SPI displays hooked to the Teensy. This one, I could re-implement as HDMI to RPi or Jetson, though, so that's not a must-have. Still it's an illustrative case of the division of smarts.


jwatte [5 minutes ago] 
Also, when it comes to the nuts and bolts of the protocol, I like protocols that:


jwatte [5 minutes ago] 
1) Are easy to re-sync if losing bytes in the middle. (Translates to a few known bytes of pre-amble/header)


jwatte [4 minutes ago] 
2) Can stuff multiple individual commands into a single bigger PDU. (This translates to efficiency on UDP connections.)


jwatte [4 minutes ago] 
3) Use stronger error checking than "nothing" or "simple sum of bytes." (This currently translates to CRC-16 for me, I'm open to others like CRC-32 or whatever)


jwatte [2 minutes ago] 
4) Is entirely asynchronous-message based. Any synchronous "I will send this request AND WAIT for a response" is a recipe for bad.

