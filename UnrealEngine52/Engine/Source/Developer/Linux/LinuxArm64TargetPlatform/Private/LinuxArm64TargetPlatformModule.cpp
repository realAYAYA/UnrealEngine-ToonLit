// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxArm64NoEditorTargetPlatformModule.cpp: Implements the FLinuxArm64NoEditorTargetPlatformModule class.
=============================================================================*/

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Interfaces/ITargetPlatformModule.h"

#include "LinuxTargetDevice.h"
#include "LinuxTargetPlatform.h"


/**
 * Module for the Linux target platforms
 */
class FLinuxArm64TargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms) override
	{
		// Game TP
		TargetPlatforms.Add(new TLinuxTargetPlatform<FLinuxPlatformProperties<false, false, false, true> >());
		// Server TP
		TargetPlatforms.Add(new TLinuxTargetPlatform<FLinuxPlatformProperties<false, true, false, true> >());
		// Client TP
		TargetPlatforms.Add(new TLinuxTargetPlatform<FLinuxPlatformProperties<false, false, true, true> >());
	}
};


IMPLEMENT_MODULE(FLinuxArm64TargetPlatformModule, LinuxArm64TargetPlatform);
