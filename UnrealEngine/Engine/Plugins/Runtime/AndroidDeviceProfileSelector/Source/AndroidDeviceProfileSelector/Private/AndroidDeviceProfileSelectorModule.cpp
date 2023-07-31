// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidDeviceProfileSelectorModule.h"
#include "AndroidDeviceProfileSelector.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FAndroidDeviceProfileSelectorModule, AndroidDeviceProfileSelector);

DEFINE_LOG_CATEGORY_STATIC(LogAndroidDPSelector, Log, All)

void FAndroidDeviceProfileSelectorModule::StartupModule()
{
}

void FAndroidDeviceProfileSelectorModule::ShutdownModule()
{
}

const FString FAndroidDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	// We are not expecting this module to have GetRuntimeDeviceProfileName called directly.
	// Android ProfileSelectorModule runtime is now in FAndroidDeviceProfileSelectorRuntimeModule.
	// Use GetDeviceProfileName.
	checkNoEntry();
	return FString();
}

const FString FAndroidDeviceProfileSelectorModule::GetDeviceProfileName()
{
	FString ProfileName; 

	// ensure SelectorProperties does actually contain our parameters
	check(FAndroidDeviceProfileSelector::GetSelectorProperties().Num() > 0);

	UE_LOG(LogAndroidDPSelector, Log, TEXT("Checking %d rules from DeviceProfile ini file."), FAndroidDeviceProfileSelector::GetNumProfiles() );
	UE_LOG(LogAndroidDPSelector, Log, TEXT("  Default profile: %s"), *ProfileName);
	for (const TTuple<FName,FString>& MapIt : FAndroidDeviceProfileSelector::GetSelectorProperties())
	{
		UE_LOG(LogAndroidDPSelector, Log, TEXT("  %s: %s"), *MapIt.Key.ToString(), *MapIt.Value);
	}

	ProfileName = FAndroidDeviceProfileSelector::FindMatchingProfile(ProfileName);

	UE_LOG(LogAndroidDPSelector, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);

	return ProfileName;
}

bool FAndroidDeviceProfileSelectorModule::GetSelectorPropertyValue(const FName& PropertyType, FString& PropertyValueOUT)
{
	if (const FString* Found = FAndroidDeviceProfileSelector::GetSelectorProperties().Find(PropertyType))
	{
		PropertyValueOUT = *Found;
		return true;
	}
	// Special case for non-existent config rule variables
	// they should return true and a value of '[null]'
	// this prevents configrule issues from throwing errors.
	if (PropertyType.ToString().StartsWith(TEXT("SRC_ConfigRuleVar[")))
	{
		PropertyValueOUT = TEXT("[null]");
		return true;
	}

	return false;
}

void FAndroidDeviceProfileSelectorModule::SetSelectorProperties(const TMap<FName, FString>& SelectorPropertiesIn)
{
	FAndroidDeviceProfileSelector::SetSelectorProperties(SelectorPropertiesIn);
}
