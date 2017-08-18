# ip_beacon -- find robots on networks

This tool will occasionally broadcast a small network 
packet on your local network, to let other programs know 
that it's here and what it's called.

## installing

To install, simply "make install" which will build the tool 
and install it and its configuration into your system in 
/usr/local/{sbin,etc} and /etc/systemd/system

## verifying status

You can verify it's present and started with systemctl:

    systemctl status ip_beacon.service

You should see something similar to this:

    ● ip_beacon.service - ip_beacon making device discoverable
       Loaded: loaded (/etc/systemd/system/ip_beacon.service; static; vendor preset: enabled)
       Active: active (running) since Fri 2017-08-18 14:11:56 PDT; 3min 45s ago
         Docs: https://gihub.com/jwatte/donkey_racing/ip_beacon/
     Main PID: 21398 (ip_beacon)
       CGroup: /system.slice/ip_beacon.service
               └─21398 /usr/local/sbin/ip_beacon

    Aug 18 14:11:56 ub16 systemd[1]: Started ip_beacon making device discoverable.

## using it

To use it, run the program in find/list mode:

    /usr/local/sbin/ip_beacon list

This is most useful when the initially installed beacon 
is running on a robot that has no display/keyboard, and 
acquires a random IP address using DHCP. Using the beacon, 
you can find it on the network. Using the randomly generated 
name (which you can re-configure in ip_beacon.ini) it is 
also easy to tell multiple robots apart, such as will happen 
with shared WiFi at meet-ups.

## Why not use the Rendez-Vous protocol?

Good question!

Next question?
