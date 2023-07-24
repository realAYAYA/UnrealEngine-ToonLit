#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

# Set your UE4 root dir here (The folder that has Engine in it)
export UE4_ENGINE_ROOT_DIR=/path/to/your/ue4/depot

# You many need to change some of these versions over time
export UE4_OPENSSL_ROOT_DIR=$UE4_ENGINE_ROOT_DIR/Engine/Source/ThirdParty/OpenSSL/1.1.1c
export UE4_EXPAT_ROOT_DIR=$UE4_ENGINE_ROOT_DIR/Engine/Source/ThirdParty/Expat/expat-2.2.10

# Actually build the platforms
echo Building Mac
cd $(pwd)/Mac
sh BuildForMac.sh
cd ..

echo Building iOS
cd $(pwd)/IOS
sh BuildForIOS.sh
cd ..
