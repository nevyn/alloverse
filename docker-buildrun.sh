#!/usr/bin/bash

rm -rf _cmake _build
sudo docker build -t alloplace .
sudo docker run -d -e ALLOPLACE_NAME -p 21337:21337/udp alloplace:latest
