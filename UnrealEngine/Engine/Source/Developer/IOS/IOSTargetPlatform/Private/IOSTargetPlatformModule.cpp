// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOSTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"


/**
 * Module for iOS as a target platform
 */
class FIOSTargetPlatformModule : public ITargetPlatformModule
{
public:

	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms) override
	{
		if (FIOSTargetPlatform::IsUsable())
		{
			TargetPlatforms.Add(new FIOSTargetPlatform(false, false, false));
			TargetPlatforms.Add(new FIOSTargetPlatform(false, false, true));
		}
	}

};


IMPLEMENT_MODULE(FIOSTargetPlatformModule, IOSTargetPlatform);
