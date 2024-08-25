// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/VCamInputSettings.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"

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

void UVCamInputSettings::PostInitProperties()
{
	Super::PostInitProperties();
	// Prior to 5.4, UVCamInputSettings used to save and load settings to ProjectName/Saved/Platform/Game.ini
	// Starting with 5.4, UVCamInputSettings saves to ProjectName/Saved/Platform/VirtualCameraCore.ini.
	
	// Our flow is a bit unusual.
	// We've got a bunch of default settings in DefaultVCamInputSettings.ini.
	// This allows us to iterate faster than setting up the default values in the C++ constructor.
	// If we want default values to change, artists can just copy the settings from Game.ini into DefaultVCamInputSettings.ini.
	// On the first run of th engine, we load from DefaultVCamInputSettings.ini.
	// After that the engine will continue to use VirtualCameraCore.ini.
	const TSharedPtr<IPlugin> VirtualCameraPlugin = IPluginManager::Get().FindPlugin(TEXT("VirtualCameraCore"));
	const FString PluginDefaultConfigFile = VirtualCameraPlugin->GetBaseDir() / TEXT("Config") / TEXT("DefaultVCamInputSettings.ini");
	const FString RealDefaultConfigFile = GetConfigFilename(this);
	
	const bool bNeedsDefaultValues = !GConfig->DoesSectionExist(TEXT("/Script/VCamCore.VCamInputSettings"), RealDefaultConfigFile);
	const bool bConfigStillExists = GConfig->DoesSectionExist(TEXT("/Script/VCamCore.VCamInputSettings"), PluginDefaultConfigFile);
	if (bNeedsDefaultValues && ensureMsgf(bConfigStillExists, TEXT("The content or location of DefaultVCamInputSettings.ini has changed")))
	{
		LoadConfig(StaticClass(), *PluginDefaultConfigFile);
	}
}

UVCamInputSettings* UVCamInputSettings::GetVCamInputSettings()
{
	return GetMutableDefault<UVCamInputSettings>();
}
