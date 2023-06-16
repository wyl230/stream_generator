#!/bin/bash
docker build -f ./sender-dockerfile -t duplex-sender:latest .
docker tag duplex-sender:latest registry.satellite.pku.edu.cn/pku-duplex-sender:v1
docker push registry.satellite.pku.edu.cn/pku-duplex-sender:v1
