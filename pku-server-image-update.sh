#!/bin/bash
g++ receiver.cpp -lpthread -o receiver
docker build -f ./receiver-dockerfile -t receiver:latest .
docker tag receiver:latest registry.satellite.pku.edu.cn/pku-server:latest
docker push registry.satellite.pku.edu.cn/pku-server:latest
