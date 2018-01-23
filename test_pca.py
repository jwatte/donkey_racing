import time
import Adafruit_PCA9685 as P

pwm = P.PCA9685()
pwm.set_pwm_freq(60)

while True:
  for i in range(300, 400):
    pwm.set_pwm(0, 0, i)
    pwm.set_pwm(1, 0, 352+((i-300)/3))
    time.sleep(0.05)
  for i in range(400, 300, -1):
    pwm.set_pwm(0, 0, i)
    pwm.set_pwm(1, 0, 352+((i-300)/3))
    time.sleep(0.05)
