// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	AndroidSplash.h: Android platform splash screen...
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSplash.h"

/**
 * Android splash implementation
 */
struct FAndroidSplash : public FGenericPlatformSplash
{
	// default implementation for now
};


typedef FAndroidSplash FPlatformSplash;
