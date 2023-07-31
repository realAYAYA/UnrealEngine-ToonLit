// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensTargetDevice.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "HoloLensTargetPlatform.h"
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FHoloLensTargetPlatformModule"


/**
 * Module for the HoloLens target platform.
 */
class FHoloLensTargetPlatformModule
	: public ITargetPlatformModule
{
public:
	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms) override
	{
		if (FHoloLensTargetPlatform::IsUsable())
		{
			//@todo HoloLens: Check for SDK?
			TargetPlatforms.Add(new FHoloLensTargetPlatform(false));
			TargetPlatforms.Add(new FHoloLensTargetPlatform(true));
		}
	}
};

#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FHoloLensTargetPlatformModule, HoloLensTargetPlatform);


#include "Windows/HideWindowsPlatformTypes.h"
