// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef PLATFORM_IOS
	#define PLATFORM_IOS 0
#endif

#ifndef PLATFORM_TVOS
	#define PLATFORM_TVOS 0
#endif

#if PLATFORM_IOS && !PLATFORM_TVOS

	// Check for Apple Vision 1.0
	#ifdef __IPHONE_11_0
		#define SUPPORTS_APPLE_VISION_1_0 1
	#else
		#define SUPPORTS_APPLE_VISION_1_0 0
	#endif

#else

	#define SUPPORTS_APPLE_VISION_1_0 0

#endif

class FAppleVisionAvailability
{
public:
	static bool SupportsAppleVision10()
	{
		static bool bSupportsAppleVision10 = false;
#if SUPPORTS_APPLE_VISION_1_0
		static bool bSupportChecked = false;
		if (!bSupportChecked)
		{
			bSupportChecked = true;
			// This call is slow, so cache the result
			bSupportsAppleVision10 = true;
		}
#endif
		return bSupportsAppleVision10;
	}
};
