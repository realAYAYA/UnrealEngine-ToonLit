#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

# Content of each /*.iconset/ folder:
# File                 Size       DPI
# -----------------------------------
# icon_16x16.png       16x16       72
# icon_16x16@2x.png    32x32      144
# icon_32x32.png       32x32       72
# icon_32x32@2x.png    64x64      144
# icon_128x128.png     128x128     72
# icon_128x128@2x.png  256x256    144
# icon_256x256.png     256x256     72
# icon_256x256@2x.png  512x512    144
# icon_512x512.png     512x512     72
# icon_512x512@2x.png  1024x1024  144
# -----------------------------------
# https://developer.apple.com/design/human-interface-guidelines/macos/icons-and-images/app-icon/

iconutil --convert icns UnrealInsights.iconset
iconutil --convert icns UTrace.iconset
