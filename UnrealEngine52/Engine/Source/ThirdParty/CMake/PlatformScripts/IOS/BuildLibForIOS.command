#!/bin/zsh -e
# Copyright Epic Games, Inc. All Rights Reserved.

# Usage: BuildLibForIOS.command <LibName> <LibVersion> [Configs] [Archs] [DeploymentTarget]

zmodload zsh/zutil

LIB_NAME=${1:?Missing library name argument}
LIB_VERSION=${2:?Missing library version argument}
LIB_CONFIGS=(Debug Release)
LIB_ARCHS=(arm64 x86_64)
LIB_DEPLOYMENT_TARGET=(11.0)
LIB_MAKE_TARGET=("")
LIB_OUTPUT_NAME=(${LIB_NAME})
LIB_CMAKE_ARGS=("")

zparseopts -D -E -K -                         \
	-config+:=LIB_CONFIGS                     \
	-arch+:=LIB_ARCHS                         \
	-deployment-target:=LIB_DEPLOYMENT_TARGET \
	-make-target:=LIB_MAKE_TARGET             \
	-output-name:=LIB_OUTPUT_NAME             \
	-cmake-args:=LIB_CMAKE_ARGS               \
	|| exit 1

LIB_CONFIGS=(${LIB_CONFIGS#--config})
LIB_CONFIGS=(${LIB_CONFIGS#=})
LIB_ARCHS=(${LIB_ARCHS#--arch})
LIB_ARCHS=(${LIB_ARCHS#=})
LIB_DEPLOYMENT_TARGET=${LIB_DEPLOYMENT_TARGET[-1]#=}
LIB_MAKE_TARGET=${LIB_MAKE_TARGET[-1]#=}
LIB_OUTPUT_NAME=${LIB_OUTPUT_NAME[-1]#=}
LIB_CMAKE_ARGS=${LIB_CMAKE_ARGS[-1]#=}

LIB_DIR=${0:a:h:h:h:h}/${LIB_NAME}/${LIB_VERSION}/lib/Mac
BIN_DIR=${0:a:h:h:h:h}/${LIB_NAME}/${LIB_VERSION}/bin/Mac
ENGINE_ROOT=${0:a:h:h:h:h:h:h}

for Config in ${LIB_CONFIGS}; do
	for Arch in ${LIB_ARCHS}; do
		echo "Building ${LIB_NAME}-${LIB_VERSION} for iOS ${Arch} in ${Config}..."
		${ENGINE_ROOT}/Build/BatchFiles/RunUAT.command BuildCMakeLib -TargetPlatform=IOS -TargetArchitecture=${Arch} -TargetLib=${LIB_NAME} -TargetLibVersion=${LIB_VERSION} -TargetConfigs=${Config} -LibOutputPath=lib -BinOutputPath=bin -CMakeGenerator=Makefile -CMakeAdditionalArguments="-DCMAKE_OSX_DEPLOYMENT_TARGET=${LIB_DEPLOYMENT_TARGET}" -SkipCreateChangelist
	done
	if [ -d ${LIB_DIR}/${LIB_ARCHS[-1]}/${Config} ]; then
		mkdir -p ${LIB_DIR}/${Config}
		for Archive in ${LIB_DIR}/${LIB_ARCHS[-1]}/${Config}/*; do
			Filename=$(basename ${Archive})
			echo "Creating universal ${Filename}..."
			lipo -create -output ${LIB_DIR}/${Config}/${Filename} ${LIB_DIR}/${^LIB_ARCHS}/${Config}/${Filename}
		done
		rm -rf ${LIB_DIR}/${^LIB_ARCHS}
	fi
	if [ -d ${BIN_DIR}/${LIB_ARCHS[-1]}/${Config} ]; then
		mkdir -p ${BIN_DIR}/${Config}
		for Archive in ${BIN_DIR}/${LIB_ARCHS[-1]}/${Config}/*; do
			echo "Creating universal ${Filename}..."
			Filename=$(basename ${Archive})
			lipo -create -output ${BIN_DIR}/${Config}/${Filename} ${BIN_DIR}/${^LIB_ARCHS}/${Config}/${Filename}
		done
		rm -rf ${BIN_DIR}/${^LIB_ARCHS}
	fi
done
