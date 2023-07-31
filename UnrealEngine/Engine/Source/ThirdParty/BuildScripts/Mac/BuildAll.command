#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

SCRIPT_DIR="`dirname "${BASH_SOURCE[0]}"`"
cd ${SCRIPT_DIR}/../..
echo Changed to $PWD

FAIL_FILE=/tmp/buildall.fail
SUCCESS_FILE=/tmp/buildall.success

rm -f ${SUCCESS_FILE} > /dev/null 2>&1
rm -f ${FAIL_FILE} > /dev/null 2>&1

multiple_cmd() { 
    logfile=$(echo $1 | tr / _)
    logfile=$(echo $logfile | tr . _)
    logfile="$logfile.txt"
    echo "Building $1 (logfile: $logfile)"
    bash -c "$1 > $logfile 2>&1"
    retVal=$?
    if [ $retVal -ne 0 ]; then
        echo "$1 Failed to build"
        echo "$1 Failed to build" > /tmp/buildall.fail
    else
        echo "$1 was rebuilt"
        echo "$1 was rebuilt" >> /tmp/buildall.success
    fi    
}; 

export -f multiple_cmd; 



find . -name 'BuildForMac.command' -exec bash -c 'multiple_cmd "$0"' {} \;
#multiple_cmd ./libOpus/opus-1.1/BuildForUE/Mac/BuildForMac.sh

if [ -f $SUCCESS_FILE ]; then
    echo '********************************************************'
    echo 'The following libraries were rebuilt -'
    cat ${SUCCESS_FILE}
    echo '********************************************************'
fi

if [ -f $FAIL_FILE ]; then
    echo '********************************************************'
    echo 'The following libraries failed to build -'
    cat ${FAIL_FILE}
    echo '********************************************************'
fi
