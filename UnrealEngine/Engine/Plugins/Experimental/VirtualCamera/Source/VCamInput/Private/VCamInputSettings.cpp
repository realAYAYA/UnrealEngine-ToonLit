// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamInputSettings.h"

bool FVCamInputProfile::operator==(const FVCamInputProfile& OtherProfile) const
{
	bool bIsEqual = MappableKeyOverrides.Num() == OtherProfile.MappableKeyOverrides.Num();
	if (bIsEqual)
	{
		for (const TPair<FName, FKey>& MappingPair : MappableKeyOverrides)
		{
			const FKey* OtherKey = OtherProfile.MappableKeyOverrides.Find(MappingPair.Key);
			if (!OtherKey || MappingPair.Value != *OtherKey)
			{
				bIsEqual = false;
				break;
			}
		}
	}
	return bIsEqual;
}

void UVCamInputSettings::SetDefaultInputProfile(const FName NewDefaultInputProfile)
{
	if (InputProfiles.Contains(NewDefaultInputProfile))
	{
		DefaultInputProfile = NewDefaultInputProfile;
		SaveConfig();
	}
}

void UVCamInputSettings::SetInputProfiles(const TMap<FName, FVCamInputProfile>& NewInputProfiles)
{
	InputProfiles = NewInputProfiles;
	SaveConfig();
}

TArray<FName> UVCamInputSettings::GetInputProfileNames() const
{
	TArray<FName> ProfileNames;
	InputProfiles.GenerateKeyArray(ProfileNames);
	return ProfileNames;
}

UVCamInputSettings* UVCamInputSettings::GetVCamInputSettings()
{
	return GetMutableDefault<UVCamInputSettings>();
}
