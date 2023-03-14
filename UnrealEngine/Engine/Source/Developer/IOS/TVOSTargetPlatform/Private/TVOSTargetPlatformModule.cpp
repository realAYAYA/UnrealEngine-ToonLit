// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOSTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"


/**
 * Module for TVOS as a target platform
 */
class FTVOSTargetPlatformModule	: public ITargetPlatformModule
{
public:

	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms) override
	{
		if (FIOSTargetPlatform::IsUsable())
		{
			// add Game and Client TPs
			TargetPlatforms.Add(new FIOSTargetPlatform(true, false));
			TargetPlatforms.Add(new FIOSTargetPlatform(true, true));
		}
	}

};


IMPLEMENT_MODULE(FTVOSTargetPlatformModule, TVOSTargetPlatform);
