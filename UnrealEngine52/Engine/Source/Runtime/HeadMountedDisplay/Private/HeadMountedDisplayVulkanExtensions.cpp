// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHeadMountedDisplayVulkanExtensions.h"
#include "GeneralProjectSettings.h"
#include "Misc/CommandLine.h"

bool IHeadMountedDisplayVulkanExtensions::ShouldDisableVulkanVSync() const
{
	// This can be called while setting up a vulkan swap chain for the PreLoadScreenManager but we cannot easily inspect bStartInVR then because the object has not been loaded (it being PreLoad).
	// For now we are just going to return false here and avoid an assert when trying to GetDefault.  This might break VR rendering of PreLoadScreens... but there is a good chance it is already broken.
	// We check for the class being loaded first so that -vr behavior is consistent with bStartInVR.
	return IsClassLoaded<UGeneralProjectSettings>() && (FParse::Param(FCommandLine::Get(), TEXT("vr")) || GetDefault<UGeneralProjectSettings>()->bStartInVR);
}
