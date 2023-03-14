#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)

export ARCH="aarch64"
export IMAGE="multiarch/centos:aarch64-clean"

${SCRIPT_DIR}/BUILD.EPIC.CentOS-7.sh
