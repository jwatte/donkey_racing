#!/bin/bash

set -e
set -u

function doit() {
    echo "Uninstall: $@"
    "$@"
}

doit sudo systemctl stop ip_beacon.service || echo "Continuing anyway"
doit sudo systemctl disable ip_beacon.service
doit sudo rm /usr/local/etc/ip_beacon.ini
doit sudo rm /usr/local/bin/ip_beacon
doit sudo rm -f /etc/systemd/ip_beacon.service

echo "------------------------------------------------"
echo "ip_beacon has been uninstalled from this system."
