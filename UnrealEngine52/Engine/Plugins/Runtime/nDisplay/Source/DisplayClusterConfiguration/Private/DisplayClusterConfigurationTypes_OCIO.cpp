// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigurationTypes_OCIO.h"

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationOCIOConfiguration
///////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterConfigurationOCIOConfiguration::IsEnabled() const
{
	return bIsEnabled && ColorConfiguration.IsValid();
}

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationOCIOProfile
///////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterConfigurationOCIOProfile::IsEnabled() const
{
	return bIsEnabled && ColorConfiguration.IsValid();
}

bool FDisplayClusterConfigurationOCIOProfile::IsEnabledForObject(const FString& InObjectId) const
{
	if (IsEnabled())
	{
		for (const FString& ViewportNameIt : ApplyOCIOToObjects)
		{
			if (InObjectId.Compare(ViewportNameIt, ESearchCase::IgnoreCase) == 0)
			{
				return true;
			}
		}
	}

	return false;
}
