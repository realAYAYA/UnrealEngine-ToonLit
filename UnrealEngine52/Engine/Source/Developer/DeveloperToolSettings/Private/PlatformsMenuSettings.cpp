// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/PlatformsMenuSettings.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Interfaces/IProjectManager.h"
#include "DesktopPlatformModule.h"
#include "DeveloperToolSettingsDelegates.h"
#include "InstalledPlatformInfo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlatformsMenuSettings)

#define LOCTEXT_NAMESPACE "SettingsClasses"

extern const FTargetInfo* FindBestTargetInfo(const FString& TargetName, bool bContentOnlyUsesEngineTargets, bool* bOutIsProjectTarget);


UPlatformsMenuSettings::UPlatformsMenuSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


const FTargetInfo* UPlatformsMenuSettings::GetBuildTargetInfoForPlatform(FName PlatformName, bool& bOutIsProjectTarget) const
{
	return FindBestTargetInfo(GetBuildTargetForPlatform(PlatformName), true, &bOutIsProjectTarget);
}

EProjectPackagingBuildConfigurations UPlatformsMenuSettings::GetBuildConfigurationForPlatform(FName PlatformName) const
{
	const EProjectPackagingBuildConfigurations* Value = PerPlatformBuildConfig.Find(PlatformName);

	// PPBC_MAX defines the default project setting case and should be handled accordingly.
	return Value == nullptr ? EProjectPackagingBuildConfigurations::PPBC_MAX : *Value;
}

void UPlatformsMenuSettings::SetBuildConfigurationForPlatform(FName PlatformName, EProjectPackagingBuildConfigurations Configuration)
{
	if (Configuration == EProjectPackagingBuildConfigurations::PPBC_MAX)
	{
		PerPlatformBuildConfig.Remove(PlatformName);
	}
	else
	{
		PerPlatformBuildConfig.Add(PlatformName, Configuration);
	}
}

FName UPlatformsMenuSettings::GetTargetFlavorForPlatform(FName FlavorName) const
{
	const FName* Value = PerPlatformTargetFlavorName.Find(FlavorName);

	// the flavor name is also the name of the vanilla info
	return Value == nullptr ? FlavorName : *Value;
}

void UPlatformsMenuSettings::SetTargetFlavorForPlatform(FName PlatformName, FName TargetFlavorName)
{
	PerPlatformTargetFlavorName.Add(PlatformName, TargetFlavorName);
}

FString UPlatformsMenuSettings::GetBuildTargetForPlatform(FName PlatformName) const
{
	const FString* Value = PerPlatformBuildTarget.Find(PlatformName);

	// empty string defines the default project setting case and should be handled accordingly.
	return Value == nullptr ? "" : *Value;
}

void UPlatformsMenuSettings::SetBuildTargetForPlatform(FName PlatformName, FString BuildTargetName)
{
	if (BuildTargetName.IsEmpty())
	{
		PerPlatformBuildTarget.Remove(PlatformName);
	}
	else
	{
		PerPlatformBuildTarget.Add(PlatformName, BuildTargetName);
	}
}

const FTargetInfo* UPlatformsMenuSettings::GetLaunchOnTargetInfo() const
{
	return FindBestTargetInfo(LaunchOnTarget, true, nullptr);
}

#undef LOCTEXT_NAMESPACE

