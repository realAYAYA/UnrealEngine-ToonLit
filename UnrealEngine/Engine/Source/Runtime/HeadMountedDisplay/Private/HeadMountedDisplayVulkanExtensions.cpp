// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHeadMountedDisplayVulkanExtensions.h"
#include "GeneralProjectSettings.h"
#include "Misc/CommandLine.h"
#include "StereoRendering.h"

bool IHeadMountedDisplayVulkanExtensions::ShouldDisableVulkanVSync() const
{
	return IStereoRendering::IsStartInVR();
}
