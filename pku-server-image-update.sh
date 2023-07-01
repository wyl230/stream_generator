#!/bin/bash
g++ receiver.cpp -lpthread -o receiver
docker build -f ./receiver-dockerfile -t receiver-pku:latest .
docker tag receiver-pku:latest registry.satellite.pku.edu.cn/pku-server-pku:latest
docker push registry.satellite.pku.edu.cn/pku-server-pku:latest
