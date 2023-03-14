// Copyright Epic Games, Inc. All Rights Reserved.

#include "Profile/MediaProfileSettingsCustomizationOptions.h"

#include "Misc/Paths.h"

FMediaProfileSettingsCustomizationOptions::FMediaProfileSettingsCustomizationOptions()
{
	ProxiesLocation.Path = TEXT("/Game/Media/Proxies/");
	NumberOfSourceProxies = 2;
	NumberOfOutputProxies = 1;
	bShouldCreateBundle = true;
	BundlesLocation.Path = TEXT("/Game/Media/Bundles");
}


bool FMediaProfileSettingsCustomizationOptions::IsValid() const
{
	return !ProxiesLocation.Path.IsEmpty()
		&& (NumberOfSourceProxies > 0 || NumberOfOutputProxies > 0)
		&& (!bShouldCreateBundle || !BundlesLocation.Path.IsEmpty());
}
