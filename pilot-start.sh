#!/bin/bash
export DISPLAY=:0
sleep 5
cd /usr/local/src/donkey_racing/pilot
mv -v /var/tmp/pilot-log-old.txt /var/tmp/pilot-log-old-2.txt
mv -v /var/tmp/pilot-log.txt /var/tmp/pilot-log-old.txt
/usr/local/src/donkey_racing/pilot/pilot --running_status=0 > /var/tmp/pilot-log.txt 2>&1 &
sudo xset s off
sudo xset -dpms
sudo xset s noblank
