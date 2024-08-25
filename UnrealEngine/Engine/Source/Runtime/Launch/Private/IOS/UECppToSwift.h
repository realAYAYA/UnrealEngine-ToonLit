// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#import <CompositorServices/CompositorServices.h>

#define SWIFT_IMPORT
// @todo do this better
#define PLATFORM_VISIONOS 1

#include "../../../ApplicationCore/Public/IOS/IOSAppDelegate.h"

class FSwiftAppBootstrap
{
public:
	static void KickoffWithCompositingLayer(CP_OBJECT_cp_layer_renderer* Layer);
};

