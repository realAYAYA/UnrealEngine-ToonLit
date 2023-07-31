#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

START_DIR=`pwd`
cd "$1"

IS_DOTNET_INSTALLED=0
DOTNET_VERSION_PATH=$(command -v dotnet) || true

if [ "$UE_USE_SYSTEM_DOTNET" == "1" ] && [ ! $DOTNET_VERSION_PATH == "" ] && [ -f $DOTNET_VERSION_PATH ]; then
	# If dotnet is installed, check that it has a new enough version of the SDK
	DOTNET_SDKS=(`dotnet --list-sdks | grep -P "(\d*)\.(\d*)\..* \[(.*)\]"`)
	for DOTNET_SDK in $DOTNET_SDKS
    do
		if [ ${DOTNET_SDK[0]} -gt 3 ]; then
			IS_DOTNET_INSTALLED=1
		fi

        if [ ${DOTNET_SDK[0]} -eq 3 ]; then
            if [ ${DOTNET_SDK[1]} -ge 1 ]; then
                IS_DOTNET_INSTALLED=1
            fi
        fi
    done
    if [ $IS_DOTNET_INSTALLED -eq 0 ]; then
    echo Unable to find installed dotnet sdk of version 3.1 or newer
    fi
fi

# Setup bundled Dotnet if cannot use installed one
if [ $IS_DOTNET_INSTALLED -eq 0 ]; then
	echo Setting up bundled DotNet SDK
	CUR_DIR=`pwd`

	# If this flag isn't set to 0, dotnet crashes during GenerateProjectFiles.sh on Ubuntu 20.04 
	export DOTNET_gcServer=0

	export UE_DOTNET_DIR="$CUR_DIR/../../../Binaries/ThirdParty/DotNet/6.0.302/linux"
	chmod u+x "$UE_DOTNET_DIR/dotnet"
	export PATH="$UE_DOTNET_DIR:$PATH"
	export DOTNET_ROOT="$UE_DOTNET_DIR"

	# We need to make sure point to our bundled libssl1, as ubuntu 22.04 is dropping libssl1 from the universe
	# as well as force override for DotNet 6 to use 1.1 over 3 as we dont have that bundled atm
	# Currently broken, need to fix!
	export CLR_OPENSSL_VERSION_OVERRIDE=1.1
	export LD_LIBRARY_PATH="$CUR_DIR/../../../Binaries/ThirdParty/OpenSSL/Unix/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH"

	# Depend on our bundled ICU vs the system. This causes issues on system that dont have the few hard coded ICU versions dotnet looks for
	export DOTNET_SYSTEM_GLOBALIZATION_APPLOCALICU=":64.1"
	export LD_LIBRARY_PATH="$CUR_DIR/../../../Binaries/ThirdParty/ICU/icu4c-64_1/lib/Unix/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH"
else
	export IS_DOTNET_INSTALLED=$IS_DOTNET_INSTALLED
fi

cd "$START_DIR"
