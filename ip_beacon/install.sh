#!/bin/bash

set -e
set -u

[ -x build/bin/ip_beacon ] || (echo "You must run this with 'make install'" && exit 1)

RANDOM_NAME=`shuf -n1 /usr/share/dict/words | tr A-Z a-z | sed -e 's/[^a-z]/-/g'`
echo "Your randomly generated name is $RANDOM_NAME."
echo "You can change this by editing /usr/local/etc/ip_beacon.ini"

function doit() {
    echo "Install: $@"
    "$@"
}

doit sudo systemctl stop ip_beacon.service || exit 0
doit sudo cp ip_beacon.ini /usr/local/etc/ip_beacon.ini
doit sudo sed -i -e "s/myname=.*/myname=$RANDOM_NAME/" /usr/local/etc/ip_beacon.ini
doit sudo cp build/bin/ip_beacon /usr/local/sbin/
doit sudo cp ip_beacon.service /etc/systemd/system/
doit sudo systemctl enable ip_beacon.service
doit sudo systemctl start ip_beacon.service

