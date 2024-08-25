#!/bin/bash

AppNameLowercase=$1
docker system prune -a -f 2>/dev/null || exit 0
docker rm -f ${AppNameLowercase}-linuxarm64-container 2>/dev/null || exit 0
docker rmi ${AppNameLowercase}-linuxarm64-image 2>/dev/null || exit 0