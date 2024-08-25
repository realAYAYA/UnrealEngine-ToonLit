// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOSTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"


/**
 * Module for TVOS as a target platform
 */
class FVisionOSTargetPlatformModule	: public ITargetPlatformModule
{
public:

	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms) override
	{
		if (FIOSTargetPlatform::IsUsable())
		{
			// add Game and Client TPs
			TargetPlatforms.Add(new FIOSTargetPlatform(false, true, false));
			TargetPlatforms.Add(new FIOSTargetPlatform(false, true, true));
		}
	}

};


IMPLEMENT_MODULE(FVisionOSTargetPlatformModule, VisionOSTargetPlatform);
