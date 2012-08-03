#!/bin/bash
cd /root/ipcam/udt-stun/udt4/app
make clean
make -f Makefile-arm 

cp stun.o /root/ipcam/app_new/stun.oo 
cp stun-camera.o /root/ipcam/app_new/stun-camera.oo 

cd /root/ipcam/udt4/cudt
make clean
make
cp udttools.o /root/ipcam/app_new/udttools.oo

cd /root/ipcam/app_new
make

echo done
